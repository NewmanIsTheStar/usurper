#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "pico/stdlib.h"  // RB Added

//pluto specific
#define NO_SYS                      (0)
#define LWIP_SOCKET                 (1)
#define PING_USE_SOCKETS            (1)      // newman added for ping test
//#define LWIP_PROVIDE_ERRNO          (1)    //was commented out
#define RECV_BUFSIZE_DEFAULT        (256)
#define DEFAULT_TCP_RECVMBOX_SIZE   (8)   //original 8 - newman set to 50
#define DEFAULT_UDP_RECVMBOX_SIZE   (8)   //original 8 - newman set to 50
#define SNTP_SUPPORT                (1)
#define SNTP_SERVER_DNS             (1)
#define SNTP_UPDATE_DELAY           (3600000)
void setTimeSec(uint32_t sec);
#define SNTP_SET_SYSTEM_TIME(sec)   setTimeSec(sec)
#define MEMP_NUM_SYS_TIMEOUT        (LWIP_NUM_SYS_TIMEOUT_INTERNAL+1)
#define LWIP_DHCP_MAX_NTP_SERVERS   (4)
//#define HTTPD_FSDATA_FILE           "htmldata.c"
#define LWIP_HTTPD_SSI_INCLUDE_TAG  (0)
#define LWIP_HTTPD_SSI              (1)
#define LWIP_HTTPD_CGI              (1)
#define DNS_TABLE_SIZE              (16)   // newman added

// generic
#if PICO_CYW43_ARCH_POLL
#define MEM_LIBC_MALLOC             1
#else
// MEM_LIBC_MALLOC is incompatible with non polling versions
#define MEM_LIBC_MALLOC             0
#endif
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    12288 //4000  // newman set to 4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define MEMP_NUM_UDP_PCB            (16)  // newman added
#define PBUF_POOL_SIZE              48    // original 24
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define TCP_WND                     16384 // newman increased for TLS (8 * TCP_MSS) 
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETCONN                0   
#define MEM_STATS                   0   
#define SYS_STATS                   0   
#define MEMP_STATS                  0   
#define LINK_STATS                  0   
// #define ETH_PAD_SIZE                2
#define LWIP_CHKSUM_ALGORITHM       3
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
// newman found these suggestions on the internet to use with keepalive
#define TCP_KEEPIDLE_DEFAULT    1000 // keep_idle : dont' send keep-alive until keep_idle after connecting
#define TCP_KEEPCNT_DEFAULT     9U // keep_cnt : increase when no response after sending keep-alive every keep_intvl

#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0
#define LWIP_IGMP                   1 // Newman added this row and enabled

// TODO Why is this conditional???
#if defined(INCORPORATE_THERMOSTAT) || defined(INCORPORATE_HOME_CONTROLLER)
#define LWIP_ALTCP               1  // Newman used for TLS testing
#define LWIP_ALTCP_TLS           1  // Newman used for TLS testing 
#define LWIP_ALTCP_TLS_MBEDTLS   1  // Newman used for TLS testing
#define ALTCP_MBEDTLS_AUTHMODE      MBEDTLS_SSL_VERIFY_NONE // MBEDTLS_SSL_VERIFY_REQUIRED -- Newman used for TLS testing
#endif

//#define NDEBUG 1 // Newman added
#ifndef NDEBUG 
//#define LWIP_DEBUG                  1
#define LWIP_STATS                  1
#define LWIP_STATS_DISPLAY          1
#endif

#define ETHARP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                  LWIP_DBG_OFF
#define API_LIB_DEBUG               LWIP_DBG_OFF
#define API_MSG_DEBUG               LWIP_DBG_OFF
#define SOCKETS_DEBUG               LWIP_DBG_OFF
#define ICMP_DEBUG                  LWIP_DBG_OFF
#define INET_DEBUG                  LWIP_DBG_OFF
#define IP_DEBUG                    LWIP_DBG_OFF
#define IP_REASS_DEBUG              LWIP_DBG_OFF
#define RAW_DEBUG                   LWIP_DBG_OFF
#define MEM_DEBUG                   LWIP_DBG_OFF
#define MEMP_DEBUG                  LWIP_DBG_OFF
#define SYS_DEBUG                   LWIP_DBG_OFF
#define TCP_DEBUG                   LWIP_DBG_OFF
#define TCP_INPUT_DEBUG             LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG            LWIP_DBG_OFF
#define TCP_RTO_DEBUG               LWIP_DBG_OFF
#define TCP_CWND_DEBUG              LWIP_DBG_OFF
#define TCP_WND_DEBUG               LWIP_DBG_OFF
#define TCP_FR_DEBUG                LWIP_DBG_OFF
#define TCP_QLEN_DEBUG              LWIP_DBG_OFF
#define TCP_RST_DEBUG               LWIP_DBG_OFF
#define UDP_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                 LWIP_DBG_OFF
#define PPP_DEBUG                   LWIP_DBG_OFF
#define SLIP_DEBUG                  LWIP_DBG_OFF
#define DHCP_DEBUG                  LWIP_DBG_OFF
#define DNS_DEBUG                   LWIP_DBG_OFF

// HTTP CLIENT TEST
//#define HTTPC_DEBUG LWIP_DBG_ON    // Newman used for httpc testing


//****************************************************************

#define TCPIP_THREAD_STACKSIZE 12288  //4096 // original 1024
#define DEFAULT_THREAD_STACKSIZE 12288  // 4096 // original 1024
#define DEFAULT_RAW_RECVMBOX_SIZE 8
#define TCPIP_MBOX_SIZE 8
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_TCP_CLOSE_TIMEOUT_MS_DEFAULT   2000 // newman added

// not necessary, can be done either way
#define LWIP_TCPIP_CORE_LOCKING_INPUT 0  // newman changed to zero

// ping_thread sets socket receive timeout, so enable this feature
#define LWIP_SO_RCVTIMEO 1


/* Debug options **************************************************************/

// // Enable lwIP debugging
// #define LWIP_DEBUG                  1

// // Enable ALTCP-compatible TLS port debugging
// #define ALTCP_MBEDTLS_DEBUG         LWIP_DBG_ON

// // Enable Mbed TLS debugging
// #define ALTCP_MBEDTLS_LIB_DEBUG     LWIP_DBG_ON

// //newman debug options
// #define MEM_DEBUG                   LWIP_DBG_ON
// #define TCP_DEBUG                   LWIP_DBG_ON

#endif
