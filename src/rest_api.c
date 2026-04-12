#include "rest_api.h"
#include "config.h"
#include "log_buffer.h"

#include <lwip/tcp.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <string.h>

#define HTTP_PORT       80
#define MAX_CONNS       8
#define REQ_BUF_SIZE    512

#define CONN_TIMEOUT_MS 5000

typedef struct
{
    struct tcp_pcb* pcb;
    char buf[REQ_BUF_SIZE]; // incoming request; reused for small response bodies
    uint16_t len;
    bool in_use;
    uint32_t opened_at_ms;
    // TX streaming
    char tx_hdr[128];
    uint16_t tx_hdr_len;
    uint16_t tx_hdr_written;
    const char* tx_body;
    int tx_body_len;
    int tx_body_written;
    uint32_t tx_bytes_acked;
} http_conn_t;

static http_conn_t conns[MAX_CONNS];

static void free_conn(http_conn_t* c) { c->in_use = false; }

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static http_conn_t* alloc_conn(struct tcp_pcb* pcb, uint32_t now_ms)
{
    for (int i = 0; i < MAX_CONNS; i++)
    {
        if (!conns[i].in_use)
        {
            conns[i].pcb = pcb;
            conns[i].len = 0;
            conns[i].in_use = true;
            conns[i].opened_at_ms = now_ms;
            return &conns[i];
        }
    }
    return NULL;
}

void rest_api_tick(uint32_t now_ms)
{
    for (int i = 0; i < MAX_CONNS; i++)
    {
        if (!conns[i].in_use)
            continue;
        if ((int32_t)(now_ms - conns[i].opened_at_ms) > CONN_TIMEOUT_MS)
        {
            printf("rest_api: closing stale connection\n");
            tcp_arg(conns[i].pcb, NULL);
            tcp_recv(conns[i].pcb, NULL);
            tcp_sent(conns[i].pcb, NULL);
            tcp_err(conns[i].pcb, NULL);
            tcp_abort(conns[i].pcb);
            free_conn(&conns[i]);
        }
    }
}

static const char* status_reason(int status)
{
    switch (status)
    {
    case 200: return "OK";
    case 404: return "Not Found";
    case 503: return "Service Unavailable";
    default: return "Bad Request";
    }
}

static void close_conn(struct tcp_pcb* tpcb, http_conn_t* conn);

// Write as many pending TX bytes as the send buffer currently allows.
// Called once to start a response and again from http_sent as ACKs free space.
// Closes the connection and returns false if tcp_write fails.
static bool pump_tx(http_conn_t* conn)
{
    while (conn->tx_hdr_written < conn->tx_hdr_len)
    {
        u16_t avail = tcp_sndbuf(conn->pcb);
        if (avail == 0)
            return true;
        u16_t todo = conn->tx_hdr_len - conn->tx_hdr_written;
        u16_t chunk = todo < avail ? todo : avail;
        bool more = (conn->tx_hdr_written + chunk < conn->tx_hdr_len)
                    || (conn->tx_body_written < conn->tx_body_len);
        u8_t flags = TCP_WRITE_FLAG_COPY | (more ? TCP_WRITE_FLAG_MORE : 0);
        if (tcp_write(conn->pcb, conn->tx_hdr + conn->tx_hdr_written, chunk, flags) != ERR_OK)
        {
            printf("rest_api: tcp_write failed, closing connection\n");
            close_conn(conn->pcb, conn);
            return false;
        }
        conn->tx_hdr_written += chunk;
    }
    while (conn->tx_body_written < conn->tx_body_len)
    {
        u16_t avail = tcp_sndbuf(conn->pcb);
        if (avail == 0)
            return true;
        int todo = conn->tx_body_len - conn->tx_body_written;
        u16_t chunk = (todo < (int)avail) ? (u16_t)todo : avail;
        bool more = (conn->tx_body_written + chunk < conn->tx_body_len);
        u8_t flags = TCP_WRITE_FLAG_COPY | (more ? TCP_WRITE_FLAG_MORE : 0);
        if (tcp_write(conn->pcb, conn->tx_body + conn->tx_body_written, chunk, flags) != ERR_OK)
        {
            printf("rest_api: tcp_write failed, closing connection\n");
            close_conn(conn->pcb, conn);
            return false;
        }
        conn->tx_body_written += chunk;
    }
    tcp_output(conn->pcb);
    // All data queued into lwIP (TCP_WRITE_FLAG_COPY) — the source buffer is
    // no longer needed since lwIP owns its own copy.
    conn->tx_body = NULL;
    return true;
}

static void send_response_impl(http_conn_t* conn, int status, const char* content_type,
                               const char* body, int blen)
{
    conn->tx_hdr_len = (uint16_t)snprintf(conn->tx_hdr, sizeof(conn->tx_hdr),
                                          "HTTP/1.0 %d %s\r\n"
                                          "Content-Type: %s\r\n"
                                          "Content-Length: %d\r\n"
                                          "Connection: close\r\n"
                                          "\r\n",
                                          status, status_reason(status), content_type, blen);
    conn->tx_hdr_written = 0;
    conn->tx_body = body;
    conn->tx_body_len = blen;
    conn->tx_body_written = 0;
    conn->tx_bytes_acked = 0;
    pump_tx(conn);
}

static void send_response(http_conn_t* conn, int status, const char* body)
{
    send_response_impl(conn, status, "text/plain", body, (int)strlen(body));
}

// ---------------------------------------------------------------------------
// Request handling
// ---------------------------------------------------------------------------

static void handle_request(http_conn_t* conn)
{
    char* req = conn->buf;

    if (strncmp(req, "GET ", 4) != 0)
    {
        send_response(conn, 400, "bad request");
        return;
    }

    char* path_start = req + 4;
    char* path_end = strstr(path_start, " HTTP");
    if (!path_end)
    {
        send_response(conn, 400, "bad request");
        return;
    }
    char path[64];
    size_t path_len = (size_t)(path_end - path_start);
    if (path_len >= sizeof(path))
        path_len = sizeof(path) - 1;
    memcpy(path, path_start, path_len);
    path[path_len] = '\0';

    if (strcmp(path, "/log") == 0)
    {
        static char snap[LOG_BUFFER_SIZE];
        for (int i = 0; i < MAX_CONNS; i++)
        {
            if (conns[i].in_use && conns[i].tx_body == snap)
            {
                send_response(conn, 503, "log busy");
                return;
            }
        }
        size_t snap_len;
        log_buffer_snapshot(snap, &snap_len);
        send_response_impl(conn, 200, "text/plain", snap, (int)snap_len);
        return;
    }

    send_response(conn, 404, "not found");
}

// ---------------------------------------------------------------------------
// lwIP TCP callbacks
// ---------------------------------------------------------------------------

static void close_conn(struct tcp_pcb* tpcb, http_conn_t* conn)
{
    tcp_arg(tpcb, NULL);
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, NULL);
    tcp_err(tpcb, NULL);
    tcp_close(tpcb);
    if (conn)
        free_conn(conn);
}

static err_t http_sent(void* arg, struct tcp_pcb* tpcb, u16_t len)
{
    http_conn_t* conn = (http_conn_t*)arg;
    conn->tx_bytes_acked += len;
    if (!pump_tx(conn))
        return ERR_OK; // pump_tx already closed the connection
    bool all_written =
     (conn->tx_hdr_written == conn->tx_hdr_len) && (conn->tx_body_written == conn->tx_body_len);
    uint32_t total = (uint32_t)conn->tx_hdr_len + (uint32_t)conn->tx_body_len;
    if (all_written && conn->tx_bytes_acked >= total)
        close_conn(tpcb, conn);
    return ERR_OK;
}

static void http_err(void* arg, err_t err)
{
    (void)err;
    if (arg)
        free_conn((http_conn_t*)arg);
}

static err_t http_recv(void* arg, struct tcp_pcb* tpcb, struct pbuf* p, err_t err)
{
    http_conn_t* conn = (http_conn_t*)arg;

    if (err != ERR_OK || p == NULL)
    {
        close_conn(tpcb, conn);
        if (p)
            pbuf_free(p);
        return ERR_OK;
    }

    uint16_t copy = p->tot_len;
    if ((uint16_t)(conn->len + copy) >= REQ_BUF_SIZE - 1)
        copy = REQ_BUF_SIZE - 1 - conn->len;
    pbuf_copy_partial(p, conn->buf + conn->len, copy, 0);
    conn->len += copy;
    conn->buf[conn->len] = '\0';
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    if (!strstr(conn->buf, "\r\n\r\n"))
        return ERR_OK; // wait for full headers

    handle_request(conn);
    return ERR_OK;
}

static err_t http_accept(void* arg, struct tcp_pcb* new_pcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK || new_pcb == NULL)
        return ERR_VAL;

    tcp_setprio(new_pcb, TCP_PRIO_MIN);

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    http_conn_t* conn = alloc_conn(new_pcb, now_ms);
    if (!conn)
    {
        tcp_abort(new_pcb);
        return ERR_ABRT;
    }

    tcp_arg(new_pcb, conn);
    tcp_recv(new_pcb, http_recv);
    tcp_sent(new_pcb, http_sent);
    tcp_err(new_pcb, http_err);
    return ERR_OK;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void rest_api_init(void)
{
    struct tcp_pcb* pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    tcp_bind(pcb, IP_ANY_TYPE, HTTP_PORT);
    pcb = tcp_listen_with_backlog(pcb, MAX_CONNS);
    tcp_accept(pcb, http_accept);
    printf("rest_api: listening on port %d\n", HTTP_PORT);
}
