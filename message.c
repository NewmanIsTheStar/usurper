
/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>


#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "generated/ws2812.pio.h"

// TODO - prune this list of includes
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "pico/util/datetime.h"
#include "hardware/rtc.h"
#include "hardware/watchdog.h"

#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/sys.h"
#include <lwip/dns.h>


#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "stdarg.h"

#include "weather.h"
#include "cgi.h"
#include "ssi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "pluto.h"
#include "led_strip.h"
#include "udp.h"
#include "message.h"
#include "message_defs.h"


//#define DEBUG_UDP_MESSAGES

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)


typedef struct LED_REMOTE_STATE_STRUCT
{
    SOCKADDR_IN resolved_address;
    TickType_t resolved_at_tick;
    int requested_pattern;
    int requested_speed;
    TickType_t requested_at_tick;
    u_int32_t latest_transaction;
    u_int32_t latest_sequence;     
    int confirmed_pattern;
    int confirmed_speed;
} LED_REMOTE_STATE_T;


//prototypes
int receive_led_strip_request(tsLED_STRIP_RQST *psMsg, SOCKADDR_IN sDest);
int receive_led_strip_confirm(tsLED_STRIP_CNFM *psMsg, SOCKADDR_IN sDest);
int send_led_strip_confirm(int iError, SOCKADDR_IN sDest, u_int32_t transaction, u_int32_t sequence);
int send_led_strip_request(int strip, int pattern, int speed, SOCKADDR_IN sDest);
void initialize_remote_led_strips(void);
void control_remote_led_strips(void);

// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;

//static variables
static LED_REMOTE_STATE_T remote_led_strip_state[6];
static SOCKET message_socket = 0;
static char message_buffer[128];
static int message_receive_timeout = 5000000;                             // five seconds
static DOUBLE_BUF_INT remote_pattern;
static DOUBLE_BUF_INT remote_speed;

/*!
 * \brief process messages sent to port 6969, format defined in message_defs.h
 *
 * \param[in]  params  alive counter that must be incremented periodically to prevent watchdog reset
 * 
 * \return nothing
 */
void message_task(__unused void *params) 
{
    SOCKADDR_IN sClientAddress;  
    int received_bytes = 0;         
    
    printf("message_task started\n");

    initialize_remote_led_strips();

    message_socket = upd_establish_socket(6969);

    if (message_socket >= 0)
    {
        for(;;)
        {
             // process messages
            received_bytes = udp_receive(message_socket, message_buffer, sizeof(message_buffer), &sClientAddress, message_receive_timeout);

            if (received_bytes >= sizeof(tsMSG_HDR))
            {
                if (check_received_header((tsMSG_HDR *)&message_buffer, sClientAddress) == 0)
                {
                    // process request
                    switch(htonl(((tsMSG_HDR *)&message_buffer)->message))
                    {
                    case LED_STRIP_RQST:
                        receive_led_strip_request((tsLED_STRIP_RQST *)&message_buffer, sClientAddress);
                        break;
                    case LED_STRIP_CNFM:
                         receive_led_strip_confirm((tsLED_STRIP_CNFM *)&message_buffer, sClientAddress);
                        break;                        
                    default:
                        printf("unrecognized Rx message ID (%lu)\n", htonl(((tsMSG_HDR *)&message_buffer)->message));
                        break;
                    }

                }
                else
                {
                    printf("unrecognized header\n");
                }
            }
            else
            {
                if (received_bytes > 0)
                {
                printf("runt packet discarded\n");
#ifdef DEBUG_UDP_MESSAGES
                    {
                        int x;
                        char *address = NULL;

                        address = inet_ntoa(sClientAddress.sin_addr);

                        printf("[%s] RX MSG = ", address);
                        for(x=0; x<received_bytes; x++) printf("%0x ", message_buffer[x]);
                        printf("\n");
                    }
#endif           
                }     
            }

            control_remote_led_strips();

            // tell watchdog task that we are still alive
            watchdog_pulse((int *)params);    
        }
    }
}



/*!
 * \brief validate message header
 *
 * \param[in]  psMsg   pointer message
 * \param[in]  sDest   address of sender
 * 
 * \return 0 if valid header, non-zero if invalid
 */
int check_received_header(tsMSG_HDR *psMsg, SOCKADDR_IN sDest)
{
    int iStatus = 0;
    char *address = NULL;

    address = inet_ntoa(sDest.sin_addr);

    // validate header
    switch(htonl(psMsg->version))
    {
        case 1:
            break;

        default:
            printf("Message header unknown version %lu recieved from %s\n", htonl(psMsg->version), address);
            iStatus = -1;
            break;
    }

    switch (htonl(psMsg->message))
    {
        case LED_STRIP_RQST:
        case LED_STRIP_CNFM:
            // TODO:  here we should detect retries and replay previous responses
            STRNCPY(web.led_last_request_ip, address, sizeof(web.led_last_request_ip));
            break;

        default:
            printf("Message header contains unrecognised MsgId %lu recieved from %s\n", htonl(psMsg->message), address);
            iStatus = -1;
            break;
    }   

    return(iStatus);
}


/*!
 * \brief set requested led strip pattern and send confirmation message
 *
 * \param[in]  psMsg   pointer message
 * \param[in]  sDest   address of sender
 * 
 * \return 0 on success
 */
int receive_led_strip_request(tsLED_STRIP_RQST *psMsg, SOCKADDR_IN sDest)
{
    int iError = 0;

    // compatibility check
    if (htonl(psMsg->sHeader.version) == 1)
    {
        // control the LED strip
        set_led_pattern_local(htonl(psMsg->pattern));
        set_led_speed_local(htonl(psMsg->speed));
   
        // send confirmation message
        send_led_strip_confirm(iError, sDest, htonl(psMsg->sHeader.transaction), htonl(psMsg->sHeader.sequence));
    }

    return EXIT_SUCCESS;
}

/*!
 * \brief record confirmed remote led strip pattern in local cache
 *
 * \param[in]  psMsg   pointer message
 * \param[in]  sDest   address of sender
 * 
 * \return 0 on success
 */
int receive_led_strip_confirm(tsLED_STRIP_CNFM *psMsg, SOCKADDR_IN sDest)
{
    //int iError = 0;
    int strip;

    // compatibility check
    if (htonl(psMsg->sHeader.version) == 1)
    {
        for (strip=0; strip < 6; strip++)
        {             
            if (remote_led_strip_state[strip].latest_transaction == htonl(psMsg->sHeader.transaction))  //TODO: this relies on transaction being unique, which is not true!
            {
                if (remote_led_strip_state[strip].latest_sequence == htonl(psMsg->sHeader.sequence))
                {
                    //printf("Got timely LED confirm from strip %d\n", strip);
                }
                else
                {
                    printf("Got late / out of order LED confirm from strip %d\n", strip);
                }

                //TODO: consider checking IP here
                remote_led_strip_state[strip].confirmed_pattern = remote_led_strip_state[strip].requested_pattern;
                remote_led_strip_state[strip].confirmed_speed = remote_led_strip_state[strip].requested_speed;
                break;
            }              

        }
}

    return EXIT_SUCCESS;
}



/*!
 * \brief send confirmed message with current led pattern
 *
 * \param[in]  psMsg   pointer message
 * \param[in]  sDest   address of requestor
 * 
 * \return number of bytes sent
 */
int send_led_strip_confirm(int iError, SOCKADDR_IN sDest, u_int32_t transaction, u_int32_t sequence)
{
    tsLED_STRIP_CNFM sCnfm;
    int iNumBytes;

    sCnfm.sHeader.version = htonl(1);
    sCnfm.sHeader.message = htonl(LED_STRIP_CNFM);
    sCnfm.sHeader.transaction = htonl(transaction);
    sCnfm.sHeader.sequence = htonl(sequence);

    sCnfm.iError = htonl(0);

    iNumBytes = udp_transmit (message_socket, (char *)&sCnfm, sizeof(tsLED_STRIP_CNFM), sDest);

    return(iNumBytes);
}


/*!
 * \brief send request to set led pattern
 *
 * \param[in]  psMsg   pointer message
 * \param[in]  sDest   address of requestor
 * 
 * \return 0 on success
 */
int send_led_strip_request(int strip, int pattern, int speed, SOCKADDR_IN sDest)
{
    tsLED_STRIP_RQST sRqst;
    int iNumBytes;
    int iError = 0;
    static int sequence = 0;
    int transaction;

    transaction = get_rand_32();

    sRqst.sHeader.version = htonl(1);
    sRqst.sHeader.message = htonl(LED_STRIP_RQST);
    sRqst.sHeader.transaction = htonl(transaction); 
    sRqst.sHeader.sequence = htonl(sequence);  
    sRqst.pattern = htonl(pattern);
    sRqst.speed = htonl(speed);  

    iNumBytes = udp_transmit (message_socket, (char *)&sRqst, sizeof(tsLED_STRIP_RQST), sDest);    

    if (iNumBytes < 0)
    {
        printf("Failed to send LED strip request\n");
        iError = 1;
    }
    else
    {
        remote_led_strip_state[strip].latest_transaction = transaction;
        remote_led_strip_state[strip].latest_sequence = sequence;   

        sequence++;     
    }

    return (iError);
}

/*!
 * \brief set remote led strip pattern -- all remote led strips will be requested to use this pattern
 *
 * \param[in]  pattern  index into pattern_table
 * 
 * \return nothing
 */
void set_led_pattern_remote(int pattern) 
{
    if (pattern < 0)
    {
        pattern = config.led_pattern;
    }

    set_double_buf_integer(&remote_pattern, pattern);
}

/*!
 * \brief set remote led strip speed -- all remote led strips will be requested to use this speed
 *
 * \param[in]  speed  milliseconds to delay before next step of led sequence
 * 
 * \return nothing
 */
void set_led_speed_remote(int speed) 
{
    if (speed < 0)
    {
        speed = config.led_speed;
    }

    set_double_buf_integer(&remote_speed, speed);
}

/*!
 * \brief construct address
 *
 * \param[in]   address_string  hostname or ip 
 * \param[in]   port            udp port
 * \param[out]  address         destination address 
 *
 * \return 0 on success
 */
int construct_address(char *address_string, int port, SOCKADDR_IN *address)
{

    int err = -1;
    struct addrinfo hints;
    struct addrinfo *result;
    int s;
    char port_string[12];
    int type;

    sprintf(port_string, "%d", port);
    type = SOCK_DGRAM;    


    /* Obtain address(es) matching host/port */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;      /* Allow IPv4 only */
    hints.ai_socktype = type;       /* Stream or Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(address_string, port_string, &hints, &result);
    if (s != 0)
    {
        printf("error returned from getaddrinfo [%s, %s, %d]\n", address_string, port_string, type);
    }
    else
    {
        if (result)
        {
            // use first dns result to construct address
            memset(address, 0, sizeof(struct sockaddr_in));
            address->sin_len = sizeof(address);
            address->sin_family = AF_INET;
            address->sin_addr.s_addr = INADDR_ANY;
            address->sin_port = PP_HTONS(port);
            
            // copy address from dns result 
            address->sin_addr = ((struct sockaddr_in *)(result->ai_addr))->sin_addr;

            err = 0;

            // delete linked list of dns results
            freeaddrinfo(result);
        } 
    }   

    return(err);
}

/*!
 * \brief Initialize remote LED strip state variables
 *
 * \param none
 * 
 * \return nothing
 */
void initialize_remote_led_strips(void)
{
    int strip;
    TickType_t tick_now;

    tick_now = xTaskGetTickCount();
    
    for(strip=0; strip<6; strip++)
    {
        memset(&(remote_led_strip_state[strip].resolved_address), 0, sizeof(struct sockaddr_in)); 
        remote_led_strip_state[strip].resolved_at_tick = tick_now;
        remote_led_strip_state[strip].requested_pattern = 0;
        remote_led_strip_state[strip].requested_speed = 0;
        remote_led_strip_state[strip].requested_at_tick = tick_now;
        remote_led_strip_state[strip].confirmed_pattern = 0;
        remote_led_strip_state[strip].confirmed_speed = 0;

        if (config.led_strip_remote_ip[strip][0])
        {
            construct_address(config.led_strip_remote_ip[strip], 6969, &(remote_led_strip_state[strip].resolved_address));
        }
    }
}


/*!
 * \brief Control remote LED strips
 *
 * \param none
 * 
 * \return nothing
 */
void control_remote_led_strips(void)
{
    int strip;
    int pattern;
    int speed;
    TickType_t tick_now;

    if (config.led_strip_remote_enable)
    {
        pattern = get_double_buf_integer(&remote_pattern, 0);
        speed = get_double_buf_integer(&remote_speed, 0);
        tick_now = xTaskGetTickCount();

        for (strip=0; strip < 6; strip++)
        {
            
            if (config.led_strip_remote_ip[strip][0])
            {
                // check time since last resolved the strip address
                if ((tick_now - remote_led_strip_state[strip].resolved_at_tick) > 60000)
                {
                    // attempt to resolve ip address
                    if (!construct_address(config.led_strip_remote_ip[strip], 6969, &(remote_led_strip_state[strip].resolved_address)))
                    {
                        remote_led_strip_state[strip].resolved_at_tick = xTaskGetTickCount();
                    }
                }
                // check if pattern and speed already confirmed and one second since last request sent
                if (((remote_led_strip_state[strip].confirmed_pattern != pattern) ||
                    (remote_led_strip_state[strip].confirmed_speed != speed)) &&
                    ((tick_now - remote_led_strip_state[strip].requested_at_tick) > 10000))
                {
                    //printf("sending led request because: pattern %d vs %d  speed %d vs %d  tick delta = %d\n", remote_led_strip_state[strip].confirmed_pattern, pattern, remote_led_strip_state[strip].confirmed_speed, speed, tick_now - remote_led_strip_state[strip].requested_at_tick);
                    // attmpt to send the message
                    if (!send_led_strip_request(strip, pattern, speed, remote_led_strip_state[strip].resolved_address))
                    {
                        remote_led_strip_state[strip].requested_pattern = pattern;
                        remote_led_strip_state[strip].requested_speed = speed;
                        remote_led_strip_state[strip].requested_at_tick = tick_now;
                    }
                }
            }

        }
    }
}

