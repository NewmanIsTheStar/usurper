/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/cyw43_arch.h"
#include "pico/types.h"
#include "pico/stdlib.h"
//#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "hardware/watchdog.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/apps/sntp.h"
#include "lwip/apps/httpd.h"
#include "dhcpserver.h"
#include "dnsserver.h"

#include "lwip/sockets.h"

#include "time.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "weather.h"
// #include "led_strip.h"
#include "cgi.h"
#include "flash.h"
#include "utility.h"
#include "config.h"
#include "watchdog.h"
#include "worker_tasks.h"
#include "wifi.h"
#include "calendar.h"
// #include "powerwall.h"
// #include "shelly.h"
// #include  "usurper_ping.h"
#include "pluto.h"

#include "ssi.h"
#ifdef USE_GIT_HASH_AS_VERSION
#include "githash.h"
#endif

#ifndef APP_NAME
#define APP_NAME "USURPER"
#endif

#ifndef RUN_FREERTOS_ON_CORE
#define RUN_FREERTOS_ON_CORE 0
#endif

#define BOSS_TASK_PRIORITY              ( 0 + 2UL )
#define WATCHDOG_TASK_PRIORITY          ( 0 + 1UL )
#define PAUSE_FOR_INITIAL_SNTP_RESPONSE (1) 


// external variables
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern WORKER_TASK_T worker_tasks[];

// static variables
static bool restart_requested = false;

long long atomic_variable = 0;

// prototypes
int pluto(void);
void boss_task(__unused void *params);
int ap_mode(void);
int set_web_ip_network_info(void);
int set_realtime_clock(void);
int monitor_stacks(void);
int test_ap_mode(void);
void atomic_write_task(__unused void *params);
void atomic_read_task(__unused void *params);
int set_gpio_defaults(void);


/*********************************************************
 * THIS MUST BE THE FIRST FUNCTION DEFINED IN THIS FILE!!!
 * *******************************************************/
int __attribute__((__section__(".applet_entry"))) main(void)
{
    pluto();

    return(0); 
}

/*!
 * \brief Initialize free RTOS and launch the first task 
 *
 * \param none
 *
 * \return 0 if scheduler stops (should never happen)
 */
int pluto(void)
{
    const char *rtos_name;
    TaskHandle_t task;

    stdio_init_all();

    printf("\n%s version ", APP_NAME);

#ifdef USE_GIT_HASH_AS_VERSION
    printf("%s\n", GITHASH);
#else
    printf("%s\n", PLUTO_VER);
#endif

    flash_get_program_size();
    printf("Compiled: %s %s\n\n",__DATE__,__TIME__);

    if (watchdog_caused_reboot())
    {
        printf("***Reboot occured***\n");
    }
    
#if ( portSUPPORT_SMP == 1 )
    rtos_name = "FreeRTOS SMP";
#else
    rtos_name = "FreeRTOS";
#endif

#if ( portSUPPORT_SMP == 1 ) && ( configNUMBER_OF_CORES == 2 )
    printf("Starting %s on both cores\n", rtos_name);
#elif ( RUN_FREERTOS_ON_CORE == 1 )
    printf("Starting %s on core 1\n", rtos_name);
#else
    printf("Starting %s on core 0\n", rtos_name);
#endif

    // initialize boss task
    xTaskCreate(boss_task, "Boss Task", 1024, NULL, BOSS_TASK_PRIORITY, &task);  

    // start boss task
    vTaskStartScheduler();

    printf("Scheduler Exited!\n");
    for(;;) sleep_ms(10000);

    // we should never get here -- if we do then drop dead
    watchdog_enable(1, 1);
    while(1);

    return 0;
}


/*!
 * \brief Start watchdog, initialize wifi, set clock, start worker tasks then monitor stacks and flash led forever
 *
 * \param params unused garbage
 *
 * \return nothing
 */
void boss_task(__unused void *params) 
{
    bool led_on = false;
    int worker = 0;
    ip_addr_t ip = {0};
    ip_addr_t nm = {0};
    ip_addr_t gw = {0};
    BaseType_t task_creation_status = 0;
    
    // start watchdog
    xTaskCreate(watchdog_task, "Watchdog Task", configMINIMAL_STACK_SIZE, NULL, WATCHDOG_TASK_PRIORITY, NULL);

    // get configuration from flash
    config_read(); 
    
    // default gpio settings  -- primarily for unused hardware connected to gpios
    set_gpio_defaults();
    
    //initialise wifi
    while (cyw43_arch_init_with_country(get_wifi_country_code(config.wifi_country)))
    {
         printf("***Failed to initialise wifi***\n");
         cyw43_arch_deinit();
         SLEEP_MS(1000);       
    } 
#ifndef TEST_AP_MODE    
    // enable wifi station mode
    cyw43_arch_enable_sta_mode();

    // set hostname
    struct netif *n = &cyw43_state.netif[CYW43_ITF_STA];
    netif_set_hostname(n, "usurper");

    //connect to wifi network
    printf("Connecting to Wi-Fi...\n");
    if (!cyw43_arch_wifi_connect_timeout_ms(config.wifi_ssid, config.wifi_password, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Connected.\n");
    } 
    else
    {
        printf("failed to connect to network.\n");

        // enable wifi access point mode -- so user can perform initial configuration
        ap_mode();
    }

    // handle static ip config
    if (!config.dhcp_enable)
    {
        printf("Using static IP settings\n");
        inet_pton(AF_INET, config.ip_address, &ip);
        inet_pton(AF_INET, config.network_mask, &nm);
        inet_pton(AF_INET, config.gateway, &gw);
        netif_set_addr(netif_default, &(ip), &(nm), &(gw));
    }
#elif
    // use this mode for development when a wifi network is not available
    test_ap_mode();
#endif    

    // initialize the ip info used in the web interface
    set_web_ip_network_info();

    printf("Pico W address = %s\n", web.ip_address_string);  
    printf("Pico W netmask = %s\n", web.network_mask_string);
    printf("Pico W gateway = %s\n", web.gateway_string);

    // initialize the clock 
    set_realtime_clock(); 

    // start web server
    init_web_variables();
    httpd_init();
    ssi_init();
    cgi_init();

    // start worker tasks
    printf("Starting worker tasks\n");       
    for(worker=0; worker_tasks[worker].functionptr != NULL; worker++)
    {
        task_creation_status = xTaskCreate(worker_tasks[worker].functionptr, worker_tasks[worker].name, worker_tasks[worker].stack_size, &(worker_tasks[worker].watchdog_alive_indicator), worker_tasks[worker].priority, &(worker_tasks[worker].task_handle));
        printf("WORKER TASK %d :: xTaskCreate returned %d\n", worker, task_creation_status);
        SLEEP_MS(1000);
    }    

    // flash the led for attention while doing no actual work (like a boss!)
    while(true) 
    {
        // toggle led
        cyw43_arch_gpio_put(0, led_on);
        led_on = !led_on;

        // copy configuration changes from RAM into flash
        config_write();

        // check stack high water mark for each worker task
        monitor_stacks();

        // report watchdog reboot to syslog server
        check_watchdog_reboot();        

        #ifdef FAKE_RTC
        // maintain fake rtc
        rtc_update();
        #endif

        SLEEP_MS(1000);

        // if (rtc_get_datetime(&date))
        // {
        //     // set day of week since ntp does not provide this TODO check if this is true and/or should be in sdk_callback.c        
        //     date.dotw = get_day_of_week(date.month, date.day, date.year);

        //     #ifndef FAKE_RTC
        //     rtc_set_datetime(&date);
        //     #endif
        // }
        // rtc_get_datetime(&date);
        // datetime_to_str(buffer, sizeof(buffer), &date);
        //printf("%s Zulu\n", buffer);


        if (restart_requested)
        {
            printf("Shutdown commenced\n");

            // disable wifi
            restart_requested = false;
            cyw43_arch_disable_sta_mode();
            cyw43_arch_deinit();
            
            // flush recent config changes to flash prior to reboot with one retry
            if (config_write()) config_write();

            printf("***REBOOT in 100 ms***\n");
            watchdog_enable(100, 0);

            SLEEP_MS(1000);
        }        
    }
}


/*!
 * \brief Request application restart 
 *
 * \param none
 *
 * \return nothing
 */
int application_restart(void)
{
    restart_requested = true;

    return(0);
}
 

/*!
 * \brief AP mode for initial configuration 
 *
 * \param none
 *
 * \return 0
 */
int ap_mode(void)
{
    bool led_on = false;
    int ap_idle = 0;
    ip_addr_t gw;
    ip4_addr_t mask;
    dhcp_server_t dhcp_server;
    dns_server_t dns_server;    

    printf("Initializing AP mode\n");
    cyw43_arch_enable_ap_mode("pluto", "",	CYW43_AUTH_OPEN); 

    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    printf("Connect to WiFi network called: pluto\nPoint web browser to http://192.168.4.1\n");

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &gw, &mask);

    // Start the dns server
    dns_server_init(&dns_server, &gw);

    // start web server to allow user to configure wifi
    init_web_variables();
    httpd_init();
    ssi_init();
    cgi_init();	

    // flash the led fast to indicate AP mode
    while(true)
    {
        // toggle led
        cyw43_arch_gpio_put(0, led_on);
        led_on = !led_on;
        
        // tell watchdog task that we are alive
        //watchdog_pulse();

        if (config_dirty(false))
        {
            config_write();
            
            //user is changing configuration
            ap_idle = 0;
        }

        // count idle cycles in AP mode
        ap_idle++;

        // reboot if in AP mode for 10 minutes without user altering configuration
        if (ap_idle > 10*60*10)
        {
            cyw43_arch_disable_ap_mode();
            cyw43_arch_deinit();

            watchdog_enable(1, 0);
        }
        else
        {
            // pat the watchdog
            //watchdog_update();
        }
        
        SLEEP_MS(100);

        if (restart_requested)
        {
            config_write();
            
            printf("***REBOOT in 100 ms***\n");
            cyw43_arch_disable_ap_mode();
            cyw43_arch_deinit();

            SLEEP_MS(100);
            
            watchdog_enable(1, 0);
        }
    } 

    return(0); 
}           



/*!
 * \brief Set IP address, netmask and gateway in web interface 
 *
 * \param none
 *
 * \return 0
 */
int set_web_ip_network_info(void)
{
    // copy ip, netmask and gateway assigned by DHCP into web interface variables
    STRNCPY(web.ip_address_string, ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[0])), sizeof(web.ip_address_string));   
    STRNCPY(web.network_mask_string, ipaddr_ntoa(netif_ip4_netmask(&cyw43_state.netif[0])), sizeof(web.network_mask_string)); 
    STRNCPY(web.gateway_string, ipaddr_ntoa(netif_ip4_gw(&cyw43_state.netif[0])), sizeof(web.gateway_string)); 

    if (config.dhcp_enable)
    {
        // set the static network config to match current DHCP assignments
        STRNCPY(config.ip_address, web.ip_address_string, sizeof(config.ip_address));
        STRNCPY(config.network_mask, web.network_mask_string, sizeof(config.network_mask));        
        STRNCPY(config.gateway, web.gateway_string, sizeof(config.gateway));    
    }

    return(0);
}


/*!
 * \brief Set the realtime clock using sntp
 *
 * \param none
 *
 * \return 0
 */
int set_realtime_clock(void)
{
    datetime_t date;
    char buffer[256]; 
    int i;   

    // initialize realtime clock
    #ifndef FAKE_RTC
    rtc_init();
    #endif
    setTimeSec(0);  //1970

    // sntp timeservers
    config_timeserver_failsafe();
    sntp_setservername(0, config.time_server[0]); 
    sntp_setservername(1, config.time_server[1]); 
    sntp_setservername(2, config.time_server[2]); 
    sntp_setservername(3, config.time_server[3]); 

    // snpt start
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();

#ifdef PAUSE_FOR_INITIAL_SNTP_RESPONSE
    //  wait up to 10 seconds for first sntp server response 
    for(i=0; i < 2*10; i++)
    {
        SLEEP_MS(500);
        // if (rtc_get_datetime(&date))
        // {
        //     // set day of week since ntp does not provide this  TODO check sdk_callback.c      
        //     date.dotw = get_day_of_week(date.month, date.day, date.year);
        //     #ifndef FAKE_RTC
        //     rtc_set_datetime(&date);
        //     #endif
        // }
        rtc_get_datetime(&date);
        datetime_to_str(buffer, sizeof(buffer), &date);
        printf("\r%s Zulu         ", buffer);

        if (date.year != 1970) break;
    }
    printf("\n");
#endif    

    return(0);
}


/*!
 * \brief Track the high water mark for each work task stack
 *
 * \param none
 *
 * \return 0
 */
int monitor_stacks(void)
{
    int worker = 0;
    int chars_remaining = 0;
    int chars_written = 0;
    int char_offset = 0;
    int spam_throttle = 0;

    // monitor stack sizes
    chars_remaining = sizeof(web.stack_message);
    chars_written = 0;
    char_offset = 0;

    for(worker=0; worker_tasks[worker].functionptr != NULL; worker++)
    {
        worker_tasks[worker].stack_high_water_mark = uxTaskGetStackHighWaterMark(worker_tasks[worker].task_handle);
        
        // print stack message for web interface
        if (chars_remaining)
        {
            chars_written = snprintf(web.stack_message+char_offset, chars_remaining, "%s=%lu  ", worker_tasks[worker].name, worker_tasks[worker].stack_high_water_mark);
            if ((chars_written >0) && (chars_written < chars_remaining))
            {
                char_offset += chars_written;
                chars_remaining = sizeof(web.stack_message) - char_offset;
            }
        }

        if (worker_tasks[worker].stack_high_water_mark < 50)
        {
            if (!spam_throttle)
            {
                send_syslog_message("usurper", "LOW STACK WARNING.  %s stack high water mark = %d", worker_tasks[worker].name, worker_tasks[worker].stack_high_water_mark);
                spam_throttle = 60;
            }
            else
            {
                spam_throttle--;
            }
        }
    } 

    return(0);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) 
{
    while(true)
    {
        printf("stack overflow\n");
    }
}

/*!
 * \brief Test AP mode used for development without access to a wifi network
 *
 * \param none
 *
 * \return 0
 */
int test_ap_mode(void)
{
    bool led_on = false;
    int ap_idle = 0;
    static ip_addr_t gw;
    static ip4_addr_t mask;
    static dhcp_server_t dhcp_server;
    static dns_server_t dns_server;    

    printf("Initializing AP mode\n");
    cyw43_arch_enable_ap_mode("pluto", "",	CYW43_AUTH_OPEN); 

    IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    printf("Connect to WiFi network called: pluto\nPoint web browser to http://192.168.4.1\n");

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &gw, &mask);

    // Start the dns server
    dns_server_init(&dns_server, &gw);

    return(0);
}

void atomic_write_task(__unused void *params)
{
    int i = 0;
    int j = 0;
    int write_core = 0;

    write_core = portGET_CORE_ID();
    
    printf("Atomic Write Test starting on core %d!\n", write_core);
    
    for(;;)
    {
        if (i)
        {
            atomic_variable = 0xAAAAAAAAAAAAAAAA;
        }
        else
        {
            atomic_variable = 0xCCCCCCCCCCCCCCCC;
        }

        i = !i;

        if (++j>=1000000000)
        {
            if (write_core != portGET_CORE_ID())
            {
                printf("WRITE CORE CHANGED!\n");
                write_core = portGET_CORE_ID();
            }
            printf("*MARK*  One billion writes\n");
            atomic_variable = 0;
            j = 0;
        }        
    }

} 

void atomic_read_task(__unused void *params)
{
    long long i = 0;
    int j = 0;
    int read_core = 0;

    read_core = portGET_CORE_ID();    

    printf("Atomic Read Test starting on core %d!\n", read_core);

    for(;;)
    {
        i = atomic_variable;

        if ((i != 0xAAAAAAAAAAAAAAAA) && (i != 0xCCCCCCCCCCCCCCCC))
        {
            printf("%llx\n", i);
            sleep_ms(1000);
        }

        if (++j>=1000000000)
        {
            if (read_core != portGET_CORE_ID())
            {
                printf("READ CORE CHANGED!\n");
                read_core = portGET_CORE_ID();
            }
            printf("*MARK*  One billion reads\n");
            j = 0;
        }
    }

} 


/*!
 * \brief GPIO defaults for pins that are unused but connected to hardware
 *
 * \param none
 *
 * \return 0
 */
int set_gpio_defaults(void)
{
    int i;

    for(i=0; i<NUM_ROWS(config.gpio_default); i++)
    {
        switch(config.gpio_default[i])
        {
        default:
        case GP_UNINITIALIZED:
            break;
        case GP_INPUT_FLOATING:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            printf("GPIO %d :: input floating\n", i);            
            break;
        case GP_INPUT_PULLED_HIGH:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            gpio_set_pulls (i, true, false);
            printf("GPIO %d :: input internally pulled high\n", i);             
            break;
        case GP_INPUT_PULLED_LOW:
            gpio_init(i);
            gpio_set_input_enabled(i, true);
            gpio_set_pulls (i, false, true);
            printf("GPIO %d :: input internally pulled low\n", i);             
            break;
        case GP_OUTPUT_HIGH:
            gpio_init(i);
            gpio_set_dir (i, true);
            gpio_put (i, true);
            printf("GPIO %d :: output high\n", i); 
            break;
        case GP_OUTPUT_LOW:
            gpio_init(i);
            gpio_set_dir (i, true);
            gpio_put (i, false);
            printf("GPIO %d :: output low\n", i);
            break;            
        }
    }
}


/*!
 * \brief print list of gpio pins with specified state
 *
 * \param none
 *
 * \return 0
 */
int print_gpio_pins_matching_default(char *buffer, int len, GPIO_DEFAULT_T gpio_default)
{
    int i;
    bool first_item_printed = false;
    int printed = 0;
    int first_in_run = -1;
    int next_in_run = -1;

    if (len > 8)
    {
        buffer[0] = 0;
        first_item_printed = false;
        for(i=0; i<NUM_ROWS(config.gpio_default) && printed < (len-8); i++)
        {
            if (config.gpio_default[i] == gpio_default)
            {               
                if (!first_item_printed)
                {
                    printed += snprintf(buffer, len, "%d", i);
                    first_item_printed = true;
                    first_in_run = i;
                    next_in_run = i+1;
                }
                else
                {
                    if ((first_in_run >= 0) && (i == next_in_run) && (i < (NUM_ROWS(config.gpio_default)-1))) // within in a run
                    {
                        next_in_run = i+1;
                    }
                    else if ((first_in_run >= 0) && (i != next_in_run) && (first_in_run+1 == next_in_run)) // end of single value run, start of new run
                    {
                        printed += snprintf(buffer+printed, len, "&#44; %d", i);
                        first_in_run = i;
                        next_in_run = i+1;
                    } 
                    else if ((first_in_run >= 0) && (i != next_in_run) && (first_in_run+1 != next_in_run)) // end of run, start of new run
                    {
                        printed += snprintf(buffer+printed, len, " - %d&#44; %d", next_in_run-1, i);
                        first_in_run = i;
                        next_in_run = i+1;
                    }
                    else if ((first_in_run >= 0) && (i == next_in_run) && (i == (NUM_ROWS(config.gpio_default)-1))) // end of list and end of run
                    {
                        printed += snprintf(buffer+printed, len, " - %d", i);
                        first_in_run = -1;
                        next_in_run = -1;
                    }                    
                    else
                    {
                        printed += snprintf(buffer+printed, len, "&#44; %d", i);
                        first_in_run = i;
                        next_in_run = i+1;
                    }                        
                }          
            }
            else if ((first_in_run >= 0) && (first_in_run+1 != next_in_run)) // end of run with more than one value
            {
                printed += snprintf(buffer+printed, len, " - %d", next_in_run-1);
                first_in_run = -1;
                next_in_run = -1;                
            }
            else if ((first_in_run >= 0)) // end of run with a single value
            {
                first_in_run = -1;
                next_in_run = -1;                
            }            
        }

        if (printed == 0)
        {
            printed += snprintf(buffer, len, "none");
        }
    }  

    return(printed);
}