/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include <hardware/flash.h>

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"


#include "stdarg.h"

#include "weather.h"
#include "flash.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"


#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

//prototype
//void establish_socket_dns_found(const char* hostname, const ip_addr_t *ipaddr, void *arg);


// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

//global
ip_addr_t dns_cache_response;  //temporary need per thread/ per call variable


// Table for the calculation of the 16-bit CRC
static const uint16_t awFcsTab[256]={

    0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,
    0x8c48,0x9dc1,0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,
    0x1081,0x0108,0x3393,0x221a,0x56a5,0x472c,0x75b7,0x643e,
    0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,0xf9ff,0xe876,
    0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,
    0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,
    0x3183,0x200a,0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,
    0xbdcb,0xac42,0x9ed9,0x8f50,0xfbef,0xea66,0xd8fd,0xc974,
    0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,0x2732,0x36bb,
    0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
    0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,
    0xdecd,0xcf44,0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,
    0x6306,0x728f,0x4014,0x519d,0x2522,0x34ab,0x0630,0x17b9,
    0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,0x8a78,0x9bf1,
    0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,
    0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,
    0x8408,0x9581,0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,
    0x0840,0x19c9,0x2b52,0x3adb,0x4e64,0x5fed,0x6d76,0x7cff,
    0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,0xf1bf,0xe036,
    0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
    0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,
    0x2942,0x38cb,0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,
    0xb58b,0xa402,0x9699,0x8710,0xf3af,0xe226,0xd0bd,0xc134,
    0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,0x5cf5,0x4d7c,
    0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,
    0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,
    0xd68d,0xc704,0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,
    0x5ac5,0x4b4c,0x79d7,0x685e,0x1ce1,0x0d68,0x3ff3,0x2e7a,
    0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,0x8238,0x93b1,
    0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
    0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,
    0x7bc7,0x6a4e,0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78
};

/*!
 * \brief Calculate CRC of a buffer
 *
 * \param pbuff[in] poointer to buffer
 * \param num_bytes[in] number of bytes to use
 * 
 * 
 * \return CRC16
 */
uint16_t crc_buffer(char *pbuff, int num_bytes)
{
    int i;
    uint16_t wResult;

    wResult = 0xffff;

    for (i = 0; i < num_bytes; i++)
    {
        wResult = (wResult >> 8) ^ awFcsTab[(wResult ^ pbuff[i]) & 0xff];
    }


    wResult ^= 0xffff;

    return(wResult);
}

/*!
 * \brief print hex dump of buffer to string
 *
 * \return nothing
 */
void hex_dump_to_string(const uint8_t *bptr, uint32_t len, char *out_string, int out_len) {
    unsigned int i = 0;
    unsigned int num_chars = 0;

    for (i=0; i<len; i++)
    {
        sprintf(out_string+num_chars, "%02x ", bptr[i]);
        
        num_chars+=3;

        if (num_chars > (out_len-4)) break;
    }
    //sprintf(out_string+num_chars, "\n");
}


/*!
 * \brief print hex dump of buffer
 *
 * \return nothing
 */
void hex_dump(const uint8_t *bptr, uint32_t len) {
    unsigned int i = 0;

    printf("dump_bytes %d", len);
    for (i = 0; i < len;i++) {
        if ((i & 0x0f) == 0) {
            printf("\n");
        } else if ((i & 0x07) == 0) {
            printf(" ");
        }
        printf("%02x ", bptr[i]);
    }
    printf("\n");

    // printf("dump_chars %d", len);
    // for (i = 0; i < len;i++) {
    //     if ((i & 0x0f) == 0) {
    //         printf("\n");
    //     } else if ((i & 0x07) == 0) {
    //         printf(" ");
    //     }
    //     if (isascii(bptr[i]))
    //     {
    //         printf("%c", bptr[i]);
    //     }
    //     else
    //     {
    //         printf("-", bptr[i]);
    //     }
    // }
    printf("\n");
}




/*!
 * \brief Create a TCP or UDP socket
 *
 * \param[in]   address_string         IPv4 address in ascii e.g. "192.168.1.1"  
 * \param[in]   port                   Port, 0 - 65535 
 * 
 * \return socket or -1 on error
 */
int establish_socket(char *address_string, struct sockaddr_in *ipv4_address, int port, int type)
{
    int socket = -1;
    struct hostent *hp;
    int err;
    int i;
    char tempaddrstring[50];


    //memset(&dns_cache_response.addr, 0, sizeof(ip_addr_t));

    printf("Attempting to create socket for %s port %d\n", address_string, port);

    for (i=0; i<3; i++)
    {
        //watchdog_pulse();

        hp = gethostbyname(address_string);  // blocking call

        //watchdog_pulse();

        if (!hp)
        {
            printf("gethostbyname() returned NULL [looking up %s]\n", address_string);
        }
        else
        {
            //printf("Got IP!\n");
            break;
        }
        sleep_ms(1000);
    }

    memset(ipv4_address, 0, sizeof(struct sockaddr_in));
    ipv4_address->sin_len = sizeof(ipv4_address);
    ipv4_address->sin_family = AF_INET;
    ipv4_address->sin_port = PP_HTONS(port);

    if(!hp)
    {
        ipv4_address->sin_addr.s_addr = inet_addr(address_string);  // string with numerical IP address only
    }
    else
    {
        ipv4_address->sin_addr.s_addr = *((u32_t *)(hp->h_addr)); //string with either hostname or numerical IP address   

        //printf("%d.%d.%d.%d\n", ((char *)(ipv4_address->sin_addr.s_addr))[0], ((char *)(ipv4_address->sin_addr.s_addr))[1], ((char *)(ipv4_address->sin_addr.s_addr))[2],((char *)(ipv4_address->sin_addr.s_addr))[3] );
    }

    socket = lwip_socket(AF_INET, type, 0);

    printf("socket = %d\n", socket);

    if (socket >= 0)
    {
        if (socket > web.socket_max) web.socket_max = socket;
        
        if (lwip_connect(socket, (struct sockaddr *)ipv4_address, sizeof(struct sockaddr_in)))
        {
            printf("closing socket due to connect failure\n");
            lwip_close(socket); 
            socket = -1;
            web.connect_failures++;
        }
    }

    return(socket);
}


/*!
 * \brief Send a syslog message
 *
 * \param[in]   log_name      name of log file on server
 * \param[in]   format, ...   variable parameters printf style  
 * 
 * \return num bytes sent or -1 on error
 */
int send_syslog_message(char *log_name, const char *format, ...)
{
    static int syslog_socket = -1;
    static struct sockaddr_in syslog_address;
    static char ip_address_string[50] = "";    
    va_list args;
    char timestamp[50];
    char syslog_message[200];
    int message_chars_remaining = 0;
    int sent_bytes = -1;

    if (config.syslog_enable)
    {
        // cache our ip address for use in syslog messages 
        if (!*ip_address_string) STRNCPY(ip_address_string, ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[0])), sizeof(ip_address_string));

        // (re)establish socket connection
        if (syslog_socket < 0) syslog_socket = establish_socket(config.syslog_server_ip, &syslog_address, 514, SOCK_DGRAM);    

        if (syslog_socket >= 0)
        {
            if (!get_timestamp(timestamp, sizeof(timestamp), true))
            {   
                message_chars_remaining = sizeof(syslog_message);
                snprintf(syslog_message, message_chars_remaining, "<165>1 %s %s %s 1 - - %%%% ", timestamp, ip_address_string, log_name);

                message_chars_remaining = sizeof(syslog_message) - strlen(syslog_message);
                va_start(args, format);  
                vsnprintf(syslog_message+strlen(syslog_message), message_chars_remaining, format, args); 
                va_end(args); 
                syslog_message[199] = 0;  // ensure string terminated

                sent_bytes = send(syslog_socket, syslog_message, strlen(syslog_message), 0);
                //printf("sent %d bytes.  MSG: %s\n", sent_bytes, syslog_message); 

                if (sent_bytes < 0)
                {
                    lwip_close(syslog_socket);
                    syslog_socket = -1;
                    web.syslog_transmit_failures++;
                }          
            }
        }
    }
    return(sent_bytes);
}


/*!
 * \brief Log watchdog reset if it occured
 * This function may be called multiple times.  It will do nothing once it has 
 * successfully sent a log message to the syslog server.  The intent is to ensure 
 * that watchdog reboots are logged even if the syslog server was not available
 * when the system started.
 * \return socket or -1 on error
 */
int check_watchdog_reboot(void)
{
    static bool watchdog_reported;

    if (!watchdog_reported && watchdog_caused_reboot())
    {
        // update web page (SSI)
        get_timestamp(web.watchdog_timestring, sizeof(web.watchdog_timestring), false);           

        // log watchdog event
        if (send_syslog_message("usurper", "REBBOT!") > 0)   //currently using watchdog to initiate all reboots, so misleading to mention watchdog in message
        {
            watchdog_reported = true;
        }
    }

    return(0);
}

/*!
 * \brief Send a govee command
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return 0 on success, non-zero on error
 */
int send_govee_command(int on, int red, int green, int blue)
{
    int err = 0;
    char timestamp[50];
    char log_message[200];
    int sent_bytes = -1;
    static int govee_socket = -1;
    static struct sockaddr_in govee_address;
    static int govee_multicast_socket = -1;
    static struct sockaddr_in govee_multicast_address;    
    fd_set readset;
    struct timeval tv;  
    int retry = 0;
    int ret = 0;
    int read_bytes = 0;
    unsigned char rx_buffer[128];  
    int p;

    // (re)establish socket connection
    if (govee_socket < 0) govee_socket = establish_socket(config.govee_light_ip, &govee_address, 4003, SOCK_DGRAM);
    //if (govee_multicast_socket < 0) govee_multicast_socket = establish_multicast_socket(&govee_multicast_address, 4002, SOCK_DGRAM);    


    if (govee_socket >= 0)
    {
        snprintf(log_message, sizeof(log_message), "{\"msg\":{\"cmd\":\"colorwc\",\"data\":{\"color\":{\"r\":%d,\"g\":%d,\"b\":%d},\"colorTemInKelvin\":0}}}\n", red, green, blue);
        //printf("Sending govee: %s\n", log_message);
        sent_bytes = send(govee_socket, log_message, strlen(log_message), 0);              

        if (sent_bytes < 0)
        {
            lwip_close(govee_socket);
            govee_socket = -1;
            err = 1;
            web.govee_transmit_failures++;
        }  

    }  

    if (govee_socket >= 0)
    {
        //for (p=0; p<10; p++){
        // snprintf(log_message, sizeof(log_message), "{\"msg\":{\"cmd\":\"devStatus\",\"data\":{}}}\n");
        // printf("Sending govee: %s\n", log_message);
        // sent_bytes = send(govee_socket, log_message, strlen(log_message), 0);              

        // if (sent_bytes < 0)
        // {
        //     lwip_close(govee_socket);
        //     govee_socket = -1;
        // }  
        // else if (sent_bytes > 0)
        // {
        //     printf("read back govee state\n");                    
        //     for (retry=0; retry<5; retry++)
        //     {
        //         FD_ZERO(&readset);
        //         FD_SET(govee_multicast_socket, &readset);
        //         tv.tv_sec = 2;
        //         tv.tv_usec = 500;

        //         printf("calling select...\n");
        //         watchdog_pulse();
        //         ret = select(govee_multicast_socket + 1, &readset, NULL, NULL, &tv);
        //         watchdog_pulse();
        //         perror("select ::");
        //         printf("returned from select with value %d\n", ret);
                

        //         if ((ret > 0) && FD_ISSET(govee_multicast_socket, &readset))
        //         {
        //             printf("calling recv...\n");
        //             read_bytes = recv(govee_multicast_socket, rx_buffer, 128, 0);
        //             printf("...returned from recv with value %d\n", read_bytes);
        //             if (read_bytes > 0)
        //             {
        //                 hex_dump(rx_buffer, read_bytes);
        //                 //TODO: function to parse govee status message
        //             }
        //             else {
        //                 perror("READ ERROR = ");
        //                 printf("read returned %d  ||| retry = %d\n", read_bytes, retry);
        //                 err = -1;
        //             }
        //         }
        //         else
        //         {
        //             printf("select returned %d  and FD_ISSET was not set  ||| retry = %d\n", ret, retry);
        //             err = -1;
        //         }  

        //     }         
        // }
        //else
        // {
        //     printf("sent_bytes = %d\n", sent_bytes);

        //     // close socket
        //     lwip_close(govee_socket);
        //     govee_socket = -1;
        //     err = -1;
        // }
        //sleep_ms(100);
        //} // for p -- debug loop
    }

    return(sent_bytes);
}

/*!
 * \brief Send a govee command
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return 0 on success, -1 on error
 */
int check_govee_state(void)
{
    char timestamp[50];
    char log_message[200];
    int sent_bytes = -1;
    static int govee_socket = -1;
    static struct sockaddr_in govee_address;

    // (re)establish socket connection
    if (govee_socket < 0) govee_socket = establish_socket(config.govee_light_ip, &govee_address, 4003, SOCK_DGRAM);

    if (govee_socket >= 0)
    {
        snprintf(log_message, sizeof(log_message), "{\"msg\":{\"cmd\":\"devStatus\",\"data\":{}}}\n");
        //printf("Sending govee: %s\n", log_message);
        sent_bytes = send(govee_socket, log_message, strlen(log_message), 0);              

        if (sent_bytes < 0)
        {
            lwip_close(govee_socket);
            govee_socket = -1;
            web.govee_transmit_failures++;
        }  

    }    

    return(sent_bytes);
}

/*!
 * \brief Create a TCP or UDP socket to receive multicast packets. NB Receive only!
 *
 * \param[in]   address_string         IPv4 address in ascii e.g. "192.168.1.1"  
 * \param[in]   port                   Port, 0 - 65535 
 * 
 * \return socket or -1 on error
 */
int establish_multicast_socket(struct sockaddr_in *ipv4_address, int port, int type)
{
    int socket = -1;
    struct hostent *hp;
    int err;
    int i;
    char tempaddrstring[50];


    //memset(&dns_cache_response.addr, 0, sizeof(ip_addr_t));

    //printf("Attempting to create socket for %s port %d\n", address_string, port);

    // for (i=0; i<3; i++)
    // {
    //     watchdog_pulse();

    //     hp = gethostbyname(address_string);  // blocking call

    //     watchdog_pulse();

    //     if (!hp)
    //     {
    //         printf("get host by name returned NULL\n");
    //     }
    //     else
    //     {
    //         printf("Got IP!\n");
    //         break;
    //     }
    //     sleep_ms(1000);
    // }

    memset(ipv4_address, 0, sizeof(struct sockaddr_in));
    ipv4_address->sin_len = sizeof(ipv4_address);
    ipv4_address->sin_family = AF_INET;
    ipv4_address->sin_port = PP_HTONS(port);  //<===== check, are we setting the source port or the destination port?


    ipv4_address->sin_addr.s_addr = INADDR_ANY;

    // if(!hp)
    // {
    //     ipv4_address->sin_addr.s_addr = inet_addr(address_string);  // string with numerical IP address only
    // }
    // else
    // {
    //     ipv4_address->sin_addr.s_addr = *((u32_t *)(hp->h_addr)); //string with either hostname or numerical IP address   

    //     //printf("%d.%d.%d.%d\n", ((char *)(ipv4_address->sin_addr.s_addr))[0], ((char *)(ipv4_address->sin_addr.s_addr))[1], ((char *)(ipv4_address->sin_addr.s_addr))[2],((char *)(ipv4_address->sin_addr.s_addr))[3] );
    // }

    socket = lwip_socket(AF_INET, type, 0);

    printf("socket = %d\n", socket);

    if (socket >= 0)
    {
        if (socket > web.socket_max) web.socket_max = socket;
        
        if (lwip_bind(socket, (struct sockaddr *)ipv4_address , sizeof(struct sockaddr_in)))
        {
            printf("closing socket due to bind failure\n");
            lwip_close(socket); 
            socket = -1;
            web.bind_failures++;
        }
        else
        {
           JoinGroup(socket, "239.255.255.250", ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[0]))); 
        }
    }

    return(socket);
}

int JoinGroup(int sock, const char* join_ip, const char* local_ip)
{
  ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(join_ip);
  mreq.imr_interface.s_addr = inet_addr(local_ip);
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0)
    return -1;
  return 0;
}


/*!
 * \brief Send a pluto command
 * 
 * \return 0 on success, -1 on error
 */
int send_pluto_message(char *message)
{
    int err = 0;
    char timestamp[50];
    char tx_buffer[200];
    int sent_bytes = -1;
    static int pluto_socket = -1;
    static struct sockaddr_in pluto_address;



    // (re)establish socket connection
    if (pluto_socket < 0) pluto_socket = establish_socket("127.0.0.1", &pluto_address, 6969, SOCK_DGRAM);

    if (pluto_socket >= 0)
    {
        snprintf(tx_buffer, sizeof(tx_buffer), "Nice!%s", message);
        printf("Sending pluto %s\n", tx_buffer);
        sent_bytes = send(pluto_socket, tx_buffer, strlen(tx_buffer), 0);              

        if (sent_bytes < 0)
        {
            lwip_close(pluto_socket);
            pluto_socket = -1;
            err = 1;
            web.pluto_transmit_failures++;
        }  

    }  

    return(sent_bytes);
}


/*!
 * \brief Set value of double buffered integer - crude alternative to using semaphores / message queues
 * 
 * \return 0 on success, non-zero on error
 */
int set_double_buf_integer(DOUBLE_BUF_INT *integer, int value)
{
    int ierr = 0;

    integer->lock++;

    integer->write_data = value;

    integer->lock--;

    return (ierr);
}

/*!
 * \brief Get value of double buffered integer - crude alternative to using semaphores / message queues
 * 
 * \return integer value -- if repeated collisions occur we return the last known value
 */
int get_double_buf_integer(DOUBLE_BUF_INT *integer, int retry)
{
    int temp;
    
    do 
    {
        if (!integer->lock)
        {
            temp = integer->write_data;

            // check for corruption by a task switch
            if (temp == integer->write_data)
            {
                integer->read_data = temp;
                break;
            }
        }
        
        if(retry > 0)
        {
            sleep_us(1);
        }
    }
    while (retry-- > 0);

    return (integer->read_data);
}

/*!
 * \brief Initialize the GPIO pin used to control the irrigation relay
 * 
 * \return 0 on success, non-zero on failure
 */
int initialize_relay_gpio_(int gpio_number)
{
    int err = -1;
    static int current_gpio_number = -1;

    switch(gpio_number)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 26:
    case 27:
    case 28:
        if (gpio_number != current_gpio_number)
        {
            if (current_gpio_number >=0)
            {
                gpio_deinit(current_gpio_number);
            }
            gpio_init(gpio_number);
            gpio_set_dir(gpio_number, GPIO_OUT);
            current_gpio_number = gpio_number;    
            err = 0;  
        }
        break;
    default:
        printf("Relay GPIO setting is invalid (%d).\n", gpio_number);
        err = -2;
        break;
    }
    return(err);
}


/*!
 * \brief Replace plus with space in string
 * 
 * \return number of evil plus signs destroyed
 */
int deplus_string(char *string, int max_len)
{
    int num_plus = 0;
    int i = 0;

    while ((i < max_len) && (string[i] != 0))
    {
        if (string[i] == '+')
        {
            string[i] = ' ';
            num_plus++;
        }

        i++;
    }

    return(num_plus);
}


/*!
 * \brief print printable text
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return number of characters printed
 */
int print_printable_text(char *contaminated_string)  
{
    int i = 0;
    int num_spaces = 0;

    if (contaminated_string != NULL)
    {
        for(i=0; i<2000; i++)
        {
            if (contaminated_string[i] == 0) break;

            if(contaminated_string[i] == '{')
            {
                printf("\n");  
                indent(num_spaces);                                  
                printf("{\n");
                num_spaces++;            
                indent(num_spaces);
            }   
            else if(contaminated_string[i] == '}')
            {
                num_spaces--;
                printf("\n");
                indent(num_spaces);            
                printf("}\n");
                indent(num_spaces);              
            }              
            else if (isprint(contaminated_string[i]))
            {
                printf("%c", contaminated_string[i]);
            }
            else if (contaminated_string[i] == '\r')
            {
                printf("[carriage return]");
            }
            else if (contaminated_string[i] == '\n')
            {
                printf("[newline]\n");
                indent(num_spaces);
            }
            
            

        }
        printf("\n");
    }
    return(i);
}

/*!
 * \brief print specfied number of spaces
 *
 * \param[in]   num_spaces  
 * 
 * \return 0
 */
int indent(int num_spaces)  
{
    while(num_spaces > 0)
    {
        printf(" ");
        num_spaces--;
    }

    return(0);
}

