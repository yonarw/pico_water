#include "mqtt_manager.h"
#include "config.h"
#include "gpio_control.h"
#include "led.h"

#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "pico/time.h"

#include <stdio.h>
#include <string.h>

#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif

// ---------------------------------------------------------------------------
// Sensor discovery table
// ---------------------------------------------------------------------------

typedef struct
{
    const char* name;
    const char* id_suffix;
    const char* val_tpl;
    const char* dev_cla;  // NULL if no device_class
    const char* unit;     // NULL if no unit_of_measurement
} sensor_def_t;

static const sensor_def_t s_sensors[] = {
    { "Firmware",    "version", "{{ value_json.version }}",  NULL,              NULL         },
    { "Uptime",      "uptime",  "{{ value_json.uptime_s }}", NULL,              "s"          },
    { "RSSI",        "rssi",    "{{ value_json.rssi_dbm }}", "signal_strength", "dBm"        },
    { "Temperature", "temp",    "{{ value_json.temp_c }}",   "temperature",     "\xc2\xb0\x43" },
};

#define SENSOR_COUNT ((int)(sizeof(s_sensors) / sizeof(s_sensors[0])))

// ---------------------------------------------------------------------------
// Boot sequence index ranges
//
// Publishes are sequenced one per tick to avoid overflowing the MQTT output
// ring buffer (MQTT_OUTPUT_RINGBUF_SIZE).
//
// [0]                            availability "online"
// [1 .. VALVE_COUNT]             switch discovery per valve
// [VALVE_COUNT+1 ..
//  VALVE_COUNT+SENSOR_COUNT]     sensor discovery per sensor field
// [VALVE_COUNT+SENSOR_COUNT+1 ..
//  2*VALVE_COUNT+SENSOR_COUNT]   initial valve states
// [2*VALVE_COUNT+SENSOR_COUNT+1] initial status
// ---------------------------------------------------------------------------

#define BOOT_AVAILABILITY  0
#define BOOT_SWITCH_FIRST  1
#define BOOT_SWITCH_LAST   (VALVE_COUNT)
#define BOOT_SENSOR_FIRST  (VALVE_COUNT + 1)
#define BOOT_SENSOR_LAST   (VALVE_COUNT + SENSOR_COUNT)
#define BOOT_VSTATE_FIRST  (VALVE_COUNT + SENSOR_COUNT + 1)
#define BOOT_VSTATE_LAST   (2 * VALVE_COUNT + SENSOR_COUNT)
#define BOOT_STATUS_IDX    (2 * VALVE_COUNT + SENSOR_COUNT + 1)
#define BOOT_SEQ_TOTAL     (2 * VALVE_COUNT + SENSOR_COUNT + 2)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

typedef enum
{
    MQTT_MGR_DISCONNECTED,
    MQTT_MGR_DNS_PENDING,
    MQTT_MGR_CONNECTING,
    MQTT_MGR_CONNECTED,
} mqtt_mgr_state_t;

static mqtt_client_t* s_client;
static mqtt_mgr_state_t s_mgr_state = MQTT_MGR_DISCONNECTED;
static ip_addr_t s_broker_addr;
static uint32_t s_next_connect_ms = 0;

static int s_boot_idx = -1;
static uint32_t s_next_status_ms = 0;

// Written from alarm IRQ (valve_tick auto-close) and lwIP IRQ (incoming cmd);
// read and cleared from main loop. Single-byte volatile is atomic on RP2040.
static volatile bool s_valve_dirty[VALVE_COUNT];
static volatile bool s_status_dirty;

static int32_t s_cached_rssi = 0;

// ---------------------------------------------------------------------------
// Temperature
// ---------------------------------------------------------------------------

static float read_temp_c(void)
{
    adc_select_input(4);
    uint16_t raw = adc_read();
    float v = raw * (3.3f / 4096.0f);
    return 27.0f - (v - 0.706f) / 0.001721f;
}

// ---------------------------------------------------------------------------
// Status JSON
// ---------------------------------------------------------------------------

static void build_status_json(char* buf, size_t size)
{
    float temp = read_temp_c();
    int ti = (int)temp;
    int tf = (int)((temp - (float)ti) * 10.0f);
    if (tf < 0)
        tf = -tf;
    uint32_t uptime_s = to_ms_since_boot(get_absolute_time()) / 1000;
    snprintf(buf, size,
             "{\"version\":\"%s\",\"uptime_s\":%u,\"rssi_dbm\":%d,"
             "\"temp_c\":%d.%d,\"active_valves\":%d}",
             FW_VERSION, (unsigned)uptime_s, (int)s_cached_rssi, ti, tf,
             valve_active_count());
}

// ---------------------------------------------------------------------------
// Publish helpers — must be called with the lwIP lock held
// ---------------------------------------------------------------------------

static err_t do_publish(const char* topic, const char* payload, u8_t retain)
{
    return mqtt_publish(s_client, topic, payload, (u16_t)strlen(payload),
                        0 /* qos */, retain, NULL, NULL);
}

static err_t publish_valve_state_locked(valve_id_t id)
{
    char topic[64];
    snprintf(topic, sizeof(topic), MQTT_TOPIC_PREFIX "/valve/%s/state", valve_names[id]);
    const char* payload = valve_get_status(id) != VALVE_STATUS_OFF ? "on" : "off";
    err_t err = do_publish(topic, payload, 0);
    if (err == ERR_OK)
        led_notify_activity();
    return err;
}

static err_t publish_status_locked(void)
{
    cyw43_wifi_get_rssi(&cyw43_state, &s_cached_rssi);
    static char buf[192];
    build_status_json(buf, sizeof(buf));
    err_t err = do_publish(MQTT_TOPIC_PREFIX "/status", buf, 0);
    if (err == ERR_OK)
        led_notify_activity();
    return err;
}

// ---------------------------------------------------------------------------
// Boot sequence state machine
//
// Returns true if the item was published successfully (caller advances index).
// Returns false if the ring buffer was full (ERR_MEM); caller retries next tick.
// ---------------------------------------------------------------------------

static bool run_boot_seq_item(int idx)
{
    static char topic[96];
    static char payload[512];

    if (idx == BOOT_AVAILABILITY)
        return do_publish(MQTT_TOPIC_PREFIX "/availability", "online", 1) == ERR_OK;

    if (idx >= BOOT_SWITCH_FIRST && idx <= BOOT_SWITCH_LAST)
    {
        const char* name = valve_names[idx - BOOT_SWITCH_FIRST];
        snprintf(topic, sizeof(topic),
                 "homeassistant/switch/" MQTT_CLIENT_ID "_%s/config", name);
        snprintf(payload, sizeof(payload),
                 "{\"name\":\"%s\","
                 "\"unique_id\":\"" MQTT_CLIENT_ID "_%s\","
                 "\"cmd_t\":\"" MQTT_TOPIC_PREFIX "/valve/%s/set\","
                 "\"stat_t\":\"" MQTT_TOPIC_PREFIX "/valve/%s/state\","
                 "\"pl_on\":\"on\",\"pl_off\":\"off\","
                 "\"avty_t\":\"" MQTT_TOPIC_PREFIX "/availability\","
                 "\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
                 "\"device\":{\"identifiers\":[\"" MQTT_CLIENT_ID "\"],"
                 "\"name\":\"Pico Water\",\"mf\":\"Raspberry Pi\","
                 "\"model\":\"Raspberry Pi Pico W\"}}",
                 name, name, name, name);
        return do_publish(topic, payload, 1) == ERR_OK;
    }

    if (idx >= BOOT_SENSOR_FIRST && idx <= BOOT_SENSOR_LAST)
    {
        const sensor_def_t* s = &s_sensors[idx - BOOT_SENSOR_FIRST];
        snprintf(topic, sizeof(topic),
                 "homeassistant/sensor/" MQTT_CLIENT_ID "_%s/config", s->id_suffix);
        int pos = snprintf(payload, sizeof(payload),
                           "{\"name\":\"%s\","
                           "\"unique_id\":\"" MQTT_CLIENT_ID "_%s\","
                           "\"stat_t\":\"" MQTT_TOPIC_PREFIX "/status\","
                           "\"val_tpl\":\"%s\","
                           "\"ent_cat\":\"diagnostic\","
                           "\"avty_t\":\"" MQTT_TOPIC_PREFIX "/availability\","
                           "\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"",
                           s->name, s->id_suffix, s->val_tpl);
        if (s->dev_cla && pos < (int)sizeof(payload))
            pos += snprintf(payload + pos, sizeof(payload) - (size_t)pos,
                            ",\"dev_cla\":\"%s\"", s->dev_cla);
        if (s->unit && pos < (int)sizeof(payload))
            pos += snprintf(payload + pos, sizeof(payload) - (size_t)pos,
                            ",\"unit_of_meas\":\"%s\"", s->unit);
        if (pos < (int)sizeof(payload))
            snprintf(payload + pos, sizeof(payload) - (size_t)pos,
                 ",\"device\":{\"identifiers\":[\"" MQTT_CLIENT_ID "\"],"
                 "\"name\":\"Pico Water\",\"mf\":\"Raspberry Pi\","
                 "\"model\":\"Raspberry Pi Pico W\"}}");
        return do_publish(topic, payload, 1) == ERR_OK;
    }

    if (idx >= BOOT_VSTATE_FIRST && idx <= BOOT_VSTATE_LAST)
        return publish_valve_state_locked((valve_id_t)(idx - BOOT_VSTATE_FIRST)) == ERR_OK;

    if (idx == BOOT_STATUS_IDX)
        return publish_status_locked() == ERR_OK;

    return true;
}

// ---------------------------------------------------------------------------
// Incoming publish (command handling)
// Callbacks are in lwIP context — no locking needed.
// ---------------------------------------------------------------------------

static char s_inpub_topic[80];
static char s_inpub_payload[8];
static u16_t s_inpub_len;

static void on_incoming_publish(void* arg, const char* topic, u32_t tot_len)
{
    (void)arg;
    (void)tot_len;
    strncpy(s_inpub_topic, topic, sizeof(s_inpub_topic) - 1);
    s_inpub_topic[sizeof(s_inpub_topic) - 1] = '\0';
    s_inpub_len = 0;
}

static void on_incoming_data(void* arg, const u8_t* data, u16_t len, u8_t flags)
{
    (void)arg;
    u16_t copy = len;
    if (s_inpub_len + copy >= (u16_t)sizeof(s_inpub_payload))
        copy = (u16_t)(sizeof(s_inpub_payload) - 1 - s_inpub_len);
    memcpy(s_inpub_payload + s_inpub_len, data, copy);
    s_inpub_len += copy;

    if (!(flags & MQTT_DATA_FLAG_LAST))
        return;

    s_inpub_payload[s_inpub_len] = '\0';

    for (int i = 0; i < VALVE_COUNT; i++)
    {
        char expected[80];
        snprintf(expected, sizeof(expected),
                 MQTT_TOPIC_PREFIX "/valve/%s/set", valve_names[i]);
        if (strcmp(s_inpub_topic, expected) != 0)
            continue;

        if (strcmp(s_inpub_payload, "on") == 0)
            valve_turn_on((valve_id_t)i);
        else if (strcmp(s_inpub_payload, "off") == 0)
            valve_turn_off((valve_id_t)i);
        else
            printf("mqtt: unknown command '%s' for valve %s\n",
                   s_inpub_payload, valve_names[i]);
        return;
    }

    if (strcmp(s_inpub_topic, MQTT_TOPIC_PREFIX "/reboot") == 0)
    {
        if (strcmp(s_inpub_payload, "1") == 0)
        {
            printf("mqtt: reboot requested\n");
            watchdog_reboot(0, 0, 100);
            while (true)
                tight_loop_contents();
        }
        return;
    }

    printf("mqtt: unhandled topic '%s'\n", s_inpub_topic);
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

static void do_connect(void);

static void on_connect(mqtt_client_t* client, void* arg, mqtt_connection_status_t status)
{
    (void)arg;
    if (status != MQTT_CONNECT_ACCEPTED)
    {
        printf("mqtt: disconnected (status %d)\n", (int)status);
        s_mgr_state = MQTT_MGR_DISCONNECTED;
        s_next_connect_ms = to_ms_since_boot(get_absolute_time()) + WIFI_RECONNECT_INTERVAL_MS;
        return;
    }

    printf("mqtt: connected to %s\n", MQTT_BROKER_HOST);
    s_mgr_state = MQTT_MGR_CONNECTED;

    mqtt_set_inpub_callback(client, on_incoming_publish, on_incoming_data, NULL);

    char topic[80];
    for (int i = 0; i < VALVE_COUNT; i++)
    {
        snprintf(topic, sizeof(topic), MQTT_TOPIC_PREFIX "/valve/%s/set", valve_names[i]);
        mqtt_sub_unsub(client, topic, 1 /* qos */, NULL, NULL, 1 /* subscribe */);
    }
    mqtt_sub_unsub(client, MQTT_TOPIC_PREFIX "/reboot", 1, NULL, NULL, 1);

    s_boot_idx = 0;
}

static void on_dns_result(const char* hostname, const ip_addr_t* addr, void* arg)
{
    (void)hostname;
    (void)arg;
    if (!addr)
    {
        printf("mqtt: DNS lookup failed for %s\n", MQTT_BROKER_HOST);
        s_mgr_state = MQTT_MGR_DISCONNECTED;
        s_next_connect_ms = to_ms_since_boot(get_absolute_time()) + WIFI_RECONNECT_INTERVAL_MS;
        return;
    }
    s_broker_addr = *addr;
    do_connect();
}

static void do_connect(void)
{
    s_mgr_state = MQTT_MGR_CONNECTING;
    struct mqtt_connect_client_info_t ci = {
        .client_id   = MQTT_CLIENT_ID,
        .client_user = MQTT_USERNAME,
        .client_pass = MQTT_PASSWORD,
        .keep_alive  = 60,
        .will_topic  = MQTT_TOPIC_PREFIX "/availability",
        .will_msg    = "offline",
        .will_qos    = 0,
        .will_retain = 1,
    };
    err_t err = mqtt_client_connect(s_client, &s_broker_addr, MQTT_BROKER_PORT,
                             on_connect, NULL, &ci);
    if (err != ERR_OK)
    {
        printf("mqtt: connect error %d\n", (int)err);
        s_mgr_state = MQTT_MGR_DISCONNECTED;
        s_next_connect_ms = to_ms_since_boot(get_absolute_time()) + WIFI_RECONNECT_INTERVAL_MS;
    }
}

static void trigger_connect(void)
{
    printf("mqtt: resolving %s...\n", MQTT_BROKER_HOST);
    s_mgr_state = MQTT_MGR_DNS_PENDING;
    ip_addr_t addr;
    err_t err = dns_gethostbyname(MQTT_BROKER_HOST, &addr, on_dns_result, NULL);
    if (err == ERR_OK)
    {
        s_broker_addr = addr;
        do_connect();
    }
    else if (err != ERR_INPROGRESS)
    {
        printf("mqtt: DNS error %d\n", (int)err);
        s_mgr_state = MQTT_MGR_DISCONNECTED;
        s_next_connect_ms = to_ms_since_boot(get_absolute_time()) + WIFI_RECONNECT_INTERVAL_MS;
    }
}

// ---------------------------------------------------------------------------
// Valve state change callback (registered with gpio_control)
// May be called from alarm IRQ (auto-close) or lwIP IRQ (incoming command).
// ---------------------------------------------------------------------------

static void on_valve_changed(valve_id_t id)
{
    s_valve_dirty[id] = true;
    s_status_dirty = true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void mqtt_manager_init(void)
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    s_client = mqtt_client_new();
    gpio_control_set_state_change_cb(on_valve_changed);
}

void mqtt_manager_tick(uint32_t now_ms)
{
    if (s_mgr_state == MQTT_MGR_DISCONNECTED)
    {
        if ((int32_t)(now_ms - s_next_connect_ms) >= 0)
        {
            cyw43_arch_lwip_begin();
            trigger_connect();
            cyw43_arch_lwip_end();
        }
        return;
    }

    if (s_mgr_state != MQTT_MGR_CONNECTED)
        return;

    cyw43_arch_lwip_begin();

    if (s_boot_idx >= 0 && s_boot_idx < BOOT_SEQ_TOTAL)
    {
        if (run_boot_seq_item(s_boot_idx))
            s_boot_idx++;
        cyw43_arch_lwip_end();
        return;
    }

    for (int i = 0; i < VALVE_COUNT; i++)
    {
        if (s_valve_dirty[i])
        {
            s_valve_dirty[i] = false;
            publish_valve_state_locked((valve_id_t)i);
        }
    }

    if (s_status_dirty || (int32_t)(now_ms - s_next_status_ms) >= 0)
    {
        s_status_dirty = false;
        s_next_status_ms = now_ms + MQTT_STATUS_INTERVAL_MS;
        publish_status_locked();
    }

    cyw43_arch_lwip_end();
}
