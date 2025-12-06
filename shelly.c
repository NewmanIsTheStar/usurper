#define _GNU_SOURCE
#include <string.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
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
#ifdef INCORPORATE_THERMOSTAT  
#include "powerwall.h"
#endif
#include "shelly.h"
#include "json_parser.h"
#include "pluto.h"
#include "usurper_ping.h"

#define GET_REQUEST "GET / HTTP/1.0\r\n\r\n"

typedef enum
{
    SHELLY_TYPE_SHSW_25,
    SHELLY_TYPE_PLUSWDUS,
    SHELLY_TYPE_PLUS1,
    SHELLY_TYPE_PLUS2PM,
    SHELLY_TYPE_UNKNOWN
} SHELLY_DEVICE_TYPE_T;

typedef struct
{
    u32_t ip;
    SHELLY_DEVICE_TYPE_T type;
} DISCOVERED_SHELLY_T;

// external variables
extern WEB_VARIABLES_T web;

// global variables
JSON_PARSER_CONTEXT_T shelly_parser_context;
unsigned char rx_buffer[2048]; 
DISCOVERED_SHELLY_T discovered_shelly[32];
int num_discovered_shelly_devices = 0;

// prototypes
int query_status(char *ipstring);
int shelly_parse_header(char *buffer);
char *find_next_space_on_line(char *buffer);
int shelly_add_discovered_device(u32_t ip, SHELLY_DEVICE_TYPE_T type);
int shelly_dump_discovered_devices(void);


/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int discover_shelly_devices(void)
{
    u32_t ip;
    u32_t mask;
    u32_t search_start;
    u32_t search_end;
    u32_t ip_to_query;
    int values[4] = {0,0,0,0};
    u8_t byte = 0;
    int i;
    char ipstring[32];
    int number_of_shelly_devices = 0;
    char device_type[32];
    char device_id[32];
    ip_addr_t ping_addr;
    int ping_err = -1;

    printf("address = %s\n", web.ip_address_string);  
    printf("netmask = %s\n", web.network_mask_string);

    sscanf(web.ip_address_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);
    printf("%d.%d.%d.%d\n", values[0], values[1], values[2], values[3]);
    ip   = 0x00000000;
    for(i=0; i<4; i++)
    {
        byte = (u8_t)values[i];
        ip = ip<<8 | byte;
    }
    printf("ip = %08x\n", ip);

    sscanf(web.network_mask_string, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);    
    printf("%d.%d.%d.%d\n", values[0], values[1], values[2], values[3]);

    mask   = 0x00000000;
    for(i=0; i<4; i++)
    {
        byte = (u8_t)values[i];
        mask = mask<<8 | byte;
    }
    printf("mask = %08x\n", mask); 

    search_start = ip & mask;
    search_end = search_start | (0xffffffff ^ mask);


    //TEST TEST TEST
    //search_start = 0xc0a821a1;

    printf("search range: %08x to %08x\n", search_start, search_end);

    for(ip_to_query=search_start+1; ip_to_query<search_end; ip_to_query++)
    {
        //byte = *((u8_t *)&ip_to_query);
        //printf("byte = %0x\n", byte);
        sprintf(ipstring, "%d.%d.%d.%d", ((u8_t *)&ip_to_query)[3], ((u8_t *)&ip_to_query)[2], ((u8_t *)&ip_to_query)[1], ((u8_t *)&ip_to_query)[0]);

        printf("querying %s\n", ipstring);

        printf("ping %s\n", ipstring);

        //ipaddr_aton(PING_ADDR, &ping_addr);
        ping_addr.addr = htonl(ip_to_query);

        ping_err = ping_device(&ping_addr, 3); 
        printf("ping returned error = %d\n", ping_err);

        if (ping_err == 0)
        {
            printf("http request %s\n", ipstring);

            //query_status(ipstring);
            if (shelly_http_request(HTTP_GET, "/shelly", ipstring, NULL) == 200)
            {
                // get device type
                if (jsonp_get_value(&shelly_parser_context, "root.\"type\"", device_type, sizeof(device_type), false))
                {
                    STRNCPY(device_type, "UNKNOWN", sizeof(device_type));
                }

                // get device id
                if (jsonp_get_value(&shelly_parser_context, "root.\"id\"", device_id, sizeof(device_id), false))
                {
                    STRNCPY(device_id, "UNKNOWN", sizeof(device_id));
                }

                if (strcasecmp(device_type, "\"SHSW-25\"") == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_SHSW_25);
                }
                else if (strncasecmp(device_id, "\"shellypluswdus", 15) == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_PLUSWDUS);
                }      
                else if (strncasecmp(device_id, "\"shellyplus1", 12) == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_PLUS1);
                }  
                else if (strncasecmp(device_id, "\"shellyplus2pm", 14) == 0)
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_PLUS2PM);
                }                                            
                else
                {
                    shelly_add_discovered_device(ip_to_query, SHELLY_TYPE_UNKNOWN);
                }

                printf("IP = %s SHELLY_DEVICE_TYPE = %s SHELLY_DEVICE_ID = %s\n", ipstring, device_type, device_id);
                shelly_dump_discovered_devices();
                
                number_of_shelly_devices++;
                printf("number of shelly devices discovered = %d\n", number_of_shelly_devices);
            }
        }
        sleep_ms(1000);
    }

    printf("number of shelly devices discovered = %d\n", number_of_shelly_devices);
    shelly_dump_discovered_devices();

    return(0);
}


// Send HTTP request
int shelly_http_request(HTTP_REQUEST_TYPE_T type, char *url, char *host, char *content)
{
    int socket = -1;
    int err = 0;
    char request[2048];
    int length = 0;
    char length_string[8];
    lwip_err_t lwip_err = -1;
    int sent_bytes = 0;
    int received_bytes = 0;
    int retry;
    int ret;
    fd_set readset;
    struct timeval tv;  
    char *start_of_json = NULL;
    int http_error = 0;
        

    // type
    switch(type)
    {
        case HTTP_GET:
            sprintf(request, "GET ");
            break;
        case HTTP_POST:
            sprintf(request, "POST ");
            break;
        default:
            request[0] = 0;
            err = true;
    }

    // url
    STRAPPEND(request, url);
    STRAPPEND(request, " HTTP/1.1\r\n");

    // host
    STRAPPEND(request, "Host: ");
    STRAPPEND(request, host);
    STRAPPEND(request, "\r\n");

    // accept
    STRAPPEND(request, "Accept: */*\r\n");

    // content
    if (content)
    {
        sprintf(length_string, "%d", strlen(content));

        // STRAPPEND(request, "Content-Type: application/json\r\n");
        // STRAPPEND(request, "Content-Length: ");
        // STRAPPEND(request, length_string);
        // STRAPPEND(request, "\r\n\r\n");
        STRAPPEND(request, content);
    }
    STRAPPEND(request, "\r\n");

    // request length 
    length = strlen(request) + 1; // TODO figure out WTF the original code is doing by transmitting one less


    if (socket < 0) socket = establish_socket(host, 80, SOCK_STREAM);

    if(socket>= 0)
    {
        printf("SEND [%d bytes]\n%s\n", length, request); 
        
        if (socket >= 0)
        {
            sent_bytes = send(socket, request, strlen(request), 0);              

            if (sent_bytes)
            {
                printf("sent_bytes = %d\n", sent_bytes);

                printf("waiting to receive response...\n");                    
                for (retry=0; retry<5; retry++)
                {
                    FD_ZERO(&readset);
                    FD_SET(socket, &readset);
                    tv.tv_sec = 2;
                    tv.tv_usec = 500;
    
                    ret = select(socket + 1, &readset, NULL, NULL, &tv);
    
                    if ((ret > 0) && FD_ISSET(socket, &readset))
                    {
                        received_bytes = recv(socket, rx_buffer, sizeof(rx_buffer), 0);

                        CLIP(received_bytes, 0, sizeof(rx_buffer) - 1);

                        // zero terminate
                        if (received_bytes < sizeof(rx_buffer)-1)
                        {
                            rx_buffer[received_bytes] = 0;
                        }
                        else
                        {
                            rx_buffer[sizeof(rx_buffer)-1] = 0;
                        }
    
                        if (received_bytes > 0)
                        {
                            hex_dump(rx_buffer, received_bytes);

                            print_printable_text(rx_buffer);

                            printf("parse header\n");
                            http_error = shelly_parse_header(rx_buffer);
                            printf("http error code = %d\n", http_error);

                            printf("parse json\n");
                            start_of_json = strcasestr(rx_buffer, "\r\n{");
                            if (start_of_json)
                            {
                                //start_of_json += 2; // point to opening brace

                                jsonp_parse_buffer(&shelly_parser_context, start_of_json, false);
                                jsonp_dump_key_value_pairs(&shelly_parser_context);
                            }

                            err = http_error;
                            break;
                        }
                        else
                        {
                            err = -2;
                        }
                    }
                    else
                    {
                        err = -3;
                    }         
                }        
            }
            else
            {
             
                err = -5;
                printf("error sending packet\n");
            }
        } 

        close(socket);
        socket = -1;
    }

    return (err);
}

// Send HTTP request
int shelly_parse_header(char *buffer)
{
    char *keyword = NULL;
    int http_error = -1;

    keyword = strcasestr(rx_buffer, "HTTP/");

    if (keyword)
    {
        keyword += strlen("HTTP/");

        keyword = find_next_space_on_line(keyword);

        if (keyword)
        {
            keyword++;

            sscanf(keyword, "%d", &http_error);            
        }
    }

    return(http_error);
}

// find next space on line
char *find_next_space_on_line(char *buffer)
{
    char *found = NULL;

    if (buffer)
    {
        while(*buffer != '\r' && *buffer != '\n' && *buffer != 0)
        {
            if (*buffer == ' ')
            {
                found = buffer;
                break;
            }

            buffer++;
        }
    }

    return(buffer);
}


// "type":"SHSW-25"/

// store discovered device
int shelly_add_discovered_device(u32_t ip, SHELLY_DEVICE_TYPE_T type)
{
    int err = 0;

    if (num_discovered_shelly_devices < NUM_ROWS(discovered_shelly))
    {
        discovered_shelly[num_discovered_shelly_devices].ip = ip;
        discovered_shelly[num_discovered_shelly_devices].type = type;

        num_discovered_shelly_devices++;
    }
    else
    {
        err = 1;
    }

    return(err);
}

// store discovered device
int shelly_dump_discovered_devices(void)
{
    int err = 0;
    int i = 0;

    if (num_discovered_shelly_devices)
    {
        for(i=0; i<num_discovered_shelly_devices; i++)
        {
            printf("IP = %d.%d.%d.%d Type = %d\n", ((u8_t *)&discovered_shelly[i].ip)[3], ((u8_t *)&discovered_shelly[i].ip)[2], ((u8_t *)&discovered_shelly[i].ip)[1], ((u8_t *)&discovered_shelly[i].ip)[0], discovered_shelly[i].type);
        }
    }

    return(err);
}
