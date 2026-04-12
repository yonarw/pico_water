#pragma once

// No RTOS
#define NO_SYS                     1
#define LWIP_SOCKET                0
#define LWIP_NETCONN               0

// Memory
#define MEM_ALIGNMENT              4
#define MEM_SIZE                   8000
#define MEMP_NUM_TCP_SEG           32
#define PBUF_POOL_SIZE             24
#define TCP_MSS                    1460
#define TCP_WND                    (8 * TCP_MSS)
#define TCP_SND_BUF                (8 * TCP_MSS)
#define TCP_SND_QUEUELEN           ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

// Protocols
#define LWIP_ARP                   1
#define LWIP_ETHERNET              1
#define LWIP_ICMP                  1
#define LWIP_DHCP                  1
#define LWIP_IPV4                  1
#define LWIP_TCP                   1
#define LWIP_UDP                   1
#define LWIP_DNS                   1
#define LWIP_TCP_KEEPALIVE         1

// Callbacks
#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK   1
#define LWIP_NETIF_HOSTNAME        1
#define LWIP_NETIF_TX_SINGLE_PBUF  1

// Checksums
#define LWIP_CHKSUM_ALGORITHM      3

// DHCP
#define DHCP_DOES_ARP_CHECK        0
#define LWIP_DHCP_DOES_ACD_CHECK   0

// MQTT output ring buffer must be >= the largest MQTT message we publish.
// Largest message is a discovery payload (~350 bytes) plus topic + header overhead.
#define MQTT_OUTPUT_RINGBUF_SIZE   1024

// Stats (disabled in release)
#define MEM_STATS                  0
#define SYS_STATS                  0
#define MEMP_STATS                 0
#define LINK_STATS                 0
