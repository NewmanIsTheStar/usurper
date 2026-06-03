/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
//#include "hardware/rtc.h"
#include "hardware/watchdog.h"
#include <hardware/flash.h>
#include "hardware/i2c.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/opt.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/apps/mqtt.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

#include "stdarg.h"

#include "watchdog.h"
#include "worker_tasks.h"
#include "weather.h"
#include "mqtt.h"
#include "flash.h"
#include "calendar.h"
#include "utility.h"
#include "config.h"
#include "pluto.h"

#define DISCOVERY_PAYLOAD_BUFFER_SIZE (2400)   // large payload sent to home assistant for automatic device discovery
#define ALL_RELAYS (8)                         // message indicating all relay states need to be published
#define CONNECTION_BACKOFF_MS_DEFAULT (500)    // minimum number of milliseconds to wait between attempt to connect to the broker

// typdedefs
typedef struct
{
    int (*initialization)(void);
    bool initialization_complete;
} MQTT_INITIALIZATION_T;

typedef enum
{
    MQTT_REASON_UNKNOWN             = 0,
    MQTT_REASON_CONNECTION_FAILED   = 1,
    MQTT_REASON_CONNECTION_DROPPED  = 2,
    MQTT_REASON_DISCONNECT          = 3,
    MQTT_REASON_TIMEOUT             = 4,
    MQTT_REASON_PUBLISH_FAILED_CB   = 5,
    MQTT_PUBLISH_FAILED_CALL        = 6,
    MQTT_REASON_CONFIG_CHANGE       = 7,

    NUM_MQTT_REASONS = 8
} MQTT_REASON_T;

// prototypes -- mqttsu_ prefix is used for local functions, whereas lwip functions use mqtt_ 
int mqttsu_sanitize_user_config(void);
int mqttsu_initialize(void);
int mqttsu_deinitialize(int (*subsytem_init_func)(void));
int mqttsu_initialize_connection(void);
void mqttsu_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
void mqttsu_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len); 
void mqttsu_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
void mqttsu_sub_request_cb(void *arg, err_t result);
void mqttsu_start_sub(mqtt_client_t *client);
void mqttsu_pub_request_cb(void *arg, err_t result);
void mqttsu_publish_discovery(mqtt_client_t *client, void *arg);
int mqttsu_initialize_subscription(void);
int mqttsu_initialize_ha_discovery(void);
int mqttsu_initialize_ha_states(void);
void mqttsu_publish_state(int relay, mqtt_client_t *client, void *arg);
int mqttsu_construct_discovery_topic(char *buffer, size_t len);
int mqttsu_construct_discovery_payload(char *buffer, size_t len);
void mqttsu_publish_all_relay_states(mqtt_client_t *client, void *arg);
int mqttsu_wait(TickType_t timeout);
void mqttsu_queue_send(uint8_t message);
int mqttsu_initialize_queue(void);
void mqttsu_publish_relay_state(int relay, mqtt_client_t *client, void *arg);
void mqttsu_tear_down(void);
void mqttsu_request_connection_restart(MQTT_REASON_T reason);
void mqttsu_end_sub(mqtt_client_t *client);
int mqttsu_keep_alive(void);
void mqttsu_sprinkler_override_begin(int zone);
void mqttsu_sprinkler_override_end(void);

// external variables
extern uint32_t unix_time;
extern NON_VOL_VARIABLES_T config;
extern WEB_VARIABLES_T web;
extern WORKER_TASK_T worker_tasks[];

// global variables
static MQTT_INITIALIZATION_T mqtt_initialization_table[] =
{
    {mqttsu_initialize_queue,                     false},     
    {mqttsu_initialize_connection,                false}, 
    {mqttsu_initialize_subscription,              false},  
    {mqttsu_initialize_ha_discovery,              false}, 
    {mqttsu_initialize_ha_states,                 false},                 
};
static bool connection_process_started = false;
static bool discovery_process_started = false;
static bool states_initialized = false;
static bool connection_completed = false;
static bool subscription_complete = false;
static bool discovery_completed = false;
static bool states_completed = false;
static int states_outstanding = 0;
static bool connection_restart_request = false;
static bool disconnect_completed = false;
static ip_addr_t broker_ip;
static int relay_to_switch = -1;
static int relay_desired_state = -1;
static QueueHandle_t mqtt_queue = NULL;                     // indicates user has change relay state
static uint8_t mqtt_message = 0;                            // relay that has changed state
static bool mqtt_queue_initialized = false;                 // queue initialization status
static mqtt_client_t *mqtt_client;
static char *homeassistant_discovery_payload = NULL;
static int connection_backoff_ms = CONNECTION_BACKOFF_MS_DEFAULT;
static MQTT_REASON_T connection_restart_reason = MQTT_REASON_UNKNOWN;
static int published_number_of_relays = 0;
static int mqtt_irrigation_override_zone = -1;

/*!
 * \brief Support relay control and monitoring via MQTT
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqtt_task(void *params)
{
    int relay_changed = 0;

    printf("MQTT task started!\n");

    // check and correct critical user configuration settings
    mqttsu_sanitize_user_config();
     
    while (true)
    {         
        // initialize all subsystems that are not already up
        mqttsu_initialize();

        // wait for timeout period but abort immediately if a relay changes state
        relay_changed = mqttsu_wait(MQTT_TASK_LOOP_DELAY);

        if (relay_changed) 
        {
            if (connection_completed)
            {
                // if a specific relay has changed then publish it first
                if ((mqtt_message >=0) && (mqtt_message < config.zone_max))
                {
                    mqttsu_publish_relay_state(mqtt_message, mqtt_client, NULL);
                }

                // publish all relay states
                mqttsu_publish_all_relay_states(mqtt_client, NULL);
            }
        }
        else
        {
            // keep broker connection alive
            mqttsu_keep_alive();
        }

        if (connection_restart_request)
        {
            printf("performing teardown reason=%d\n", connection_restart_reason);
            send_syslog_message("mqtt", "Performing teardown reason=%d", connection_restart_reason);

            mqttsu_tear_down();
            connection_restart_request = false;
        }
        // tell watchdog task that we are still alive
        watchdog_pulse((int *)params);           
    }              
}



/*!
 * \brief initialize subsystems
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_initialize(void)
{
    static bool init_complete = false;
    static int  attempt = 0;
    int err = 0;
    int i;

    attempt++;

    for (i=0; i < NUM_ROWS(mqtt_initialization_table); i++)
    {
        if (!mqtt_initialization_table[i].initialization_complete)
        {
            mqtt_initialization_table[i].initialization_complete = !mqtt_initialization_table[i].initialization();            

            if (!mqtt_initialization_table[i].initialization_complete)
            {
                err++;
                printf("MQTT incomplete initialization of subsystem %d at attempt %d\n", i, attempt);
                init_complete = false;
            }
        }
    }

    if (err)
    {
        printf("MQTT %d subsystem%s failed to initialize during attempt %d\n", err, err>1?"s":"", attempt);
        
    } else if (!init_complete)
    {
        printf("MQTT all subsystems sucessfully initialized at attempt %d\n", attempt);
        init_complete = true;
        attempt = 0;
    }

    return(err);
}

/*!
 * \brief deinitialize a subsytem
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_deinitialize(int (*subsytem_init_func)(void))
{
    int err = 1;
    int i;

    for (i=0; i < NUM_ROWS(mqtt_initialization_table); i++)
    {
        if (mqtt_initialization_table[i].initialization == subsytem_init_func)
        {
            mqtt_initialization_table[i].initialization_complete = false;
            err = 0;
            break;
        }
    }

    return(err);
}


 /*!
 * \brief perform sanity check on critical user config values
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_sanitize_user_config(void)
{   
    // force zero termination
    config.mqtt_user[sizeof(config.mqtt_user)- 1] = 0;
    config.mqtt_password[sizeof(config.mqtt_password)- 1] = 0;
    config.mqtt_broker_address[sizeof(config.mqtt_broker_address)- 1] = 0;

    return(0);
}

 /*!
 * \brief Begin MQTT connection process
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_initialize_connection(void)
{
    int err = -1;
    static struct mqtt_connect_client_info_t ci;

    // generate unique mqtt client name
    sprintf(web.mqtt_client_name, "%s-%02x-%02x-%02x-%02x-%02x-%02x", APP_NAME, web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);

    if (config.mqtt_broker_address[0] != 0)
    {
        broker_ip.addr = address_string_to_ip(config.mqtt_broker_address);
   
        if (broker_ip.addr)
        {            
            memset(&ci, 0, sizeof(ci));
            ci.client_id = web.mqtt_client_name;
            ci.client_user = config.mqtt_user;
            ci.client_pass = config.mqtt_password;
            ci.keep_alive = 30;

            cyw43_arch_lwip_begin();
            mqtt_client = mqtt_client_new();
            cyw43_arch_lwip_end();

            if (mqtt_client != NULL) 
            {
                cyw43_arch_lwip_begin();
                err = mqtt_client_connect(mqtt_client, &broker_ip, MQTT_PORT, mqttsu_connection_cb, NULL, &ci);
                cyw43_arch_lwip_end();

                //printf("mqtt connect returned %d\n", err);

                if (err == ERR_OK)
                {
                    connection_process_started = true;
                }
                else
                {
                    printf("connect attempt failed\n");
                    send_syslog_message("mqtt", "Connect attempt failed");
                }
            }
        }

        SLEEP_MS(connection_backoff_ms);        
    }
    //printf("initialize connection returning %d\n", err);

    return(err);
}

 /*!
 * \brief Begin subscription process
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_initialize_subscription(void)
{
    int err = -1;
    struct mqtt_connect_client_info_t ci;

    if (connection_completed)
    {
        // Subscribe to a topic here
        mqttsu_start_sub(mqtt_client);
        err = 0;
    }

    return(err);
}

 /*!
 * \brief Begin home assistant mqtt device discovery process
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_initialize_ha_discovery(void)
{
    int err = -1;

    if (connection_completed)
    {
        printf("sending home assitant discovery packet\n");
        send_syslog_message("mqtt", "Sending home assistant discovery packet");        

        mqttsu_publish_discovery(mqtt_client, NULL);
        
        discovery_process_started = true;

        err = 0;
    }

    //printf("initialize discovery returning %d\n", err);
    
    return(err);
}

 /*!
 * \brief Initial publication of relay states
 *
 * \param params none
 * 
 * \return 0 on success
 */
int mqttsu_initialize_ha_states(void)
{
    int err = -1;
    int j = 0;

    // sleep until discovery_completed or 5 seconds elapse
    for(j=0; (j < 100) && !discovery_completed; j++) 
    {
        SLEEP_MS(50);
    }

    if (discovery_completed)
    {
        //printf("about to call publish all states\n");

        mqttsu_publish_all_relay_states(mqtt_client, NULL);
        
        states_initialized = true;

        err = 0;
    }
    //printf("initialize states returning %d\n", err);
    
    return(err);
}

/*!
 * \brief Receive connection process completion status
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    switch(status)
    {
    case MQTT_CONNECT_ACCEPTED:
        connection_completed = true;
        connection_backoff_ms = CONNECTION_BACKOFF_MS_DEFAULT;    
        break;
    case MQTT_CONNECT_DISCONNECTED:
        disconnect_completed = true;
        if (!connection_restart_request)
        {
            mqttsu_request_connection_restart(MQTT_REASON_DISCONNECT);
        }
        break;
    case MQTT_CONNECT_TIMEOUT:
        mqttsu_request_connection_restart(MQTT_REASON_TIMEOUT);
        break;
    default:
        printf("MQTT Connection failed: %d\n", status);

        // double connection attempt backoff time up to approx 5 minutes
        if (!connection_completed && (connection_backoff_ms < 5*60000))
        {
            connection_backoff_ms *=2;
        }
        else
        {
           mqttsu_request_connection_restart(MQTT_REASON_UNKNOWN);
        }     
        break;                        
    }
}

/*!
 * \brief Receive MQTT command topic that identifies the relay
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) 
{
    char expected_domain[32];
    
    sprintf(expected_domain, "relay-c-%02x-%02x-%02x-%02x-%02x-%02x", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]); 
    
    //printf("Topic: %s, Total Length: %u\n", topic, (unsigned int)tot_len);

    if (strlen(topic) == strlen("relay-c-00-11-22-33-44-55/X/command"))
    {
        if ((strncasecmp(topic, expected_domain, strlen(expected_domain)) == 0) &&
            (strncasecmp(topic + strlen(expected_domain) + strlen("/X/"), "command", strlen("command")) == 0) &&
            isdigit(topic[strlen(expected_domain) + strlen("/")]))
        {
            relay_to_switch = topic[strlen(expected_domain) + strlen("/")] - '0' - 1;  // switch to zero base
            //printf("got relay to switch = %d\n", relay_to_switch);
        }
        else
        {
            //send_syslog_message("mqtt", "unexpected command rejected");
        }
    }
}

/*!
 * \brief Receive MQTT command data that specifies the relay state (ON or OFF)
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) 
{
    if (flags & MQTT_DATA_FLAG_LAST)  // TODO: make this accept and aggregate data received in multiple chunks
    {
        if ((relay_to_switch >=0) && (relay_to_switch < config.zone_max))
        {
            if (strncasecmp(data, "ON", 2) == 0)
            {
                web.rmtsw_relay_desired_state[relay_to_switch] = true;
                mqttsu_sprinkler_override_begin(relay_to_switch);
                mqttsu_queue_send((uint8_t)relay_to_switch);                    
            }
            else if (strncasecmp(data, "OFF", 3) == 0)
            {
                web.rmtsw_relay_desired_state[relay_to_switch] = false;
                mqttsu_sprinkler_override_end();
                mqttsu_queue_send((uint8_t)relay_to_switch);  
            }
            
            relay_to_switch = -1;
        }
       
    }
    else if (flags)
    {
        printf("unhandled partial publication payload packet \n");
        //send_syslog_message("mqtt", "unhandled partial packet");
    }
}

/*!
 * \brief Receives status of subscripton process upon completetion
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_sub_request_cb(void *arg, err_t result) 
{
    if (result == ERR_OK)
    {
        subscription_complete = true;
    }
    else
    {
        printf("Subscribe result: %d\n", result);
        //send_syslog_message("mqtt", "subscribe failed %d", result);
    }
}

/*!
 * \brief Send subscription
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_start_sub(mqtt_client_t *client)
{
    int err;
    static char topic[32];

    // Set callbacks
    cyw43_arch_lwip_begin();
    mqtt_set_inpub_callback(client, mqttsu_incoming_publish_cb, mqttsu_incoming_data_cb, NULL);
    cyw43_arch_lwip_end();

    // Subscribe
    sprintf(topic, "relay-c-%02x-%02x-%02x-%02x-%02x-%02x/#", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]); 
    cyw43_arch_lwip_begin();   
    err = mqtt_subscribe(client, topic, 1, mqttsu_sub_request_cb, NULL);    
    cyw43_arch_lwip_end();

    //printf("subscribe result = %d\n", err);
}

/*!
 * \brief Receive publication process status upon completion
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_pub_request_cb(void *arg, err_t result) 
{
    if(result != ERR_OK) 
    {
        printf("Publish failed: %d\n", result);
        //send_syslog_message("mqtt", "publish failed");
        mqttsu_request_connection_restart(MQTT_REASON_PUBLISH_FAILED_CB);
    } else 
    {
        //printf("Publish success\n");
    }

    if (arg)
    {
        //printf("publish callback received argument %d\n", *(MQTT_CALLBACK_ID_T *)arg);
        switch(*(MQTT_CALLBACK_ID_T *)arg)
        {
        case MQTT_CALLBACK_DISCOVERY_ID:
            if (homeassistant_discovery_payload)
            {
                free(homeassistant_discovery_payload);
                homeassistant_discovery_payload = NULL;
                discovery_completed = true;
                published_number_of_relays = config.zone_max;
            }
            break;
        case MQTT_CALLBACK_STATE_ID:
            if (states_outstanding > 0)
            {
                states_outstanding--;
            }
            states_completed = true;
            break;    
        default:        
            break;
        }
    } 
}

/*!
 * \brief Send discovery publication
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_publish_discovery(mqtt_client_t *client, void *arg)
{
    //const char *pub_payload = "Pico2W Hello!";
    err_t err;
    u8_t qos = 1; // 0, 1, or 2
    u8_t retain = 0;
    //static char discovery_payload[DISCOVERY_PAYLOAD_BUFFER_SIZE];
    static char discovery_topic[60];
    static MQTT_CALLBACK_ID_T discovery_arg = MQTT_CALLBACK_DISCOVERY_ID;
    
    mqttsu_construct_discovery_topic(discovery_topic, sizeof(discovery_topic));
   
    if (!homeassistant_discovery_payload)
    {
        homeassistant_discovery_payload = malloc(DISCOVERY_PAYLOAD_BUFFER_SIZE);
    }

    if (homeassistant_discovery_payload)
    {    
        mqttsu_construct_discovery_payload(homeassistant_discovery_payload, DISCOVERY_PAYLOAD_BUFFER_SIZE);

        if (published_number_of_relays != config.zone_max)
        {
            // remove device from home assistant
            retain = 1;
            cyw43_arch_lwip_begin();
            err = mqtt_publish(client, discovery_topic, "", 0, qos, retain, mqttsu_pub_request_cb, arg);
            cyw43_arch_lwip_end();

            if(err != ERR_OK) 
            {
                printf("Publish discovery error while removing device from home assistant: %d\n", err);
            }

            SLEEP_MS(1000);
        }

        // add device to home assistant
        retain = 1;
        cyw43_arch_lwip_begin();
        err = mqtt_publish(client, discovery_topic, homeassistant_discovery_payload, strlen(homeassistant_discovery_payload), qos, retain, mqttsu_pub_request_cb, &discovery_arg);
        cyw43_arch_lwip_end();;

        if(err != ERR_OK) 
        {
            printf("Publish discovery error while adding device to home asssitant: %d\n", err);
        }
    }
    else
    {
        printf("failed to send home assistant device discovery topic due to lack of memory\n");
    }
}

/*!
 * \brief Send relay state publication
 *
 * \param relay 0 - 7
 * 
 * \return nothing
 */
void mqttsu_publish_state(int relay, mqtt_client_t *client, void *arg)
{
    const char *pub_payload = "Pico2W Hello!";
    err_t err;
    u8_t qos = 2; // 0, 1, or 2
    u8_t retain = 0;
    char state[64];
    char state_payload[8];
    static MQTT_CALLBACK_ID_T state_arg = MQTT_CALLBACK_STATE_ID;

    CLIP(relay, 0, 7);

    sprintf(state, "relay-s-%02x-%02x-%02x-%02x-%02x-%02x/%d/state", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5], relay+1);

    if (web.rmtsw_relay_desired_state[relay])
    {
        sprintf(state_payload, "ON");
    }
    else
    {
        sprintf(state_payload, "OFF");
    }

    // send state
    retain = 0;
    cyw43_arch_lwip_begin();
    err = mqtt_publish(client, state, state_payload, strlen(state_payload), qos, retain, mqttsu_pub_request_cb, &state_arg);
    cyw43_arch_lwip_end();

    if(err != ERR_OK) 
    {
        printf("Publish state error: %d\n", err);
        //send_syslog_message("mqtt", "publish state error %d", err);
        mqttsu_request_connection_restart(MQTT_PUBLISH_FAILED_CALL);
    }

    //printf("published new state. %s = %s\n", state, state_payload);
}

/*!
 * \brief print home assistant discovery payload into callers buffer
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
int mqttsu_construct_discovery_payload(char *buffer, size_t len)
{
    int err = 0;
    int i; 
    char temp_string[64];

    *buffer = 0;

    STRNCAT(buffer, "{\"dev\": {\"ids\": \"", len);
    sprintf(temp_string, "rs-%02x-%02x-%02x-%02x-%02x-%02x", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);
    STRNCAT(buffer, temp_string, len);
    STRNCAT(buffer, "\",\"name\": \"", len); 
    STRNCAT(buffer, config.host_name, len);
    STRNCAT(buffer, "\"},\"o\": {\"name\":\"", len);
    STRNCAT(buffer, config.host_name, len);
    STRNCAT(buffer, "\",\"sw\": \"", len);
    STRNCAT(buffer, PLUTO_VER, len);
    STRNCAT(buffer, "\",\"url\": \"https://github.com/NewmanIsTheStar/remote-switch/wiki\"},\"cmps\": {", len);
    for(i=0; i<config.zone_max; i++)
    {
        STRNCAT(buffer, "\"", len);
        sprintf(temp_string, "rs-r%d-%02x-%02x-%02x-%02x-%02x-%02x", i+1, web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);
        STRNCAT(buffer, temp_string, len);
        STRNCAT(buffer, "\": {\"p\": \"switch\",\"command_topic\":\"", len);
        sprintf(temp_string, "relay-c-%02x-%02x-%02x-%02x-%02x-%02x/%d/command", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5], i+1);
        STRNCAT(buffer, temp_string, len);
        STRNCAT(buffer, "\",\"state_topic\":\"", len);
        sprintf(temp_string, "relay-s-%02x-%02x-%02x-%02x-%02x-%02x/%d/state", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5], i+1);
        STRNCAT(buffer, temp_string, len);
        STRNCAT(buffer, "\",\"unique_id\":\"", len);
        sprintf(temp_string, "rs-id%d-%02x-%02x-%02x-%02x-%02x-%02x", i+1, web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);
        STRNCAT(buffer, temp_string, len);
        STRNCAT(buffer, "\",\"name\":\"", len); 
        sprintf(temp_string, "Zone %d", i+1);  
        STRNCAT(buffer, temp_string, len);    // TODO change to config.zone_name[i] once added to web ui
        STRNCAT(buffer, "\"}", len);         
        if (i < (config.zone_max -1))
        {
            STRNCAT(buffer, ",", len);  // home assistant is very fussy about trailing commas
        }
        
    }
    STRNCAT(buffer, "},\"qos\": 0}", len);

    return(err);
}

/*!
 * \brief print home assistant discovery topic into callers buffer
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
int mqttsu_construct_discovery_topic(char *buffer, size_t len)
{
    int err = 0;
    int i; 
    char temp_string[32];

    *buffer = 0;

    STRNCAT(buffer, "homeassistant/device/rmtsw-", len);
    sprintf(temp_string, "%02x-%02x-%02x-%02x-%02x-%02x", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]);
    STRNCAT(buffer, temp_string, len);
    STRNCAT(buffer, "/config", len);  
    
    return(0);
}

/*!
 * \brief send all relay states to the mqtt broker sequentially
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_publish_all_relay_states(mqtt_client_t *client, void *arg)
{
    int i = 0;
    int j = 0;

    states_outstanding = 0;

    for(i=0; i<config.zone_max; i++)
    {
        states_outstanding++;
        mqttsu_publish_state(i, client, arg);

        // sleep until callback decrements states_outstanding to 0 or 5 seconds elapse
        for(j=0; (j < 100) && states_outstanding; j++) 
        {
            SLEEP_MS(50);
        }

        // abort if we did not get callback within 5 seconds
        if (j>=100) break;
    }
}

/*!
 * \brief send relay state to the mqtt broker and wait for callback confirmation
 *
 * \param relay 0 - 7
 * 
 * \return nothing
 */
void mqttsu_publish_relay_state(int relay, mqtt_client_t *client, void *arg)
{
    int j = 0;

    if ((relay >= 0) && (relay < config.zone_max))
    {
        states_outstanding = 1;
        mqttsu_publish_state(relay, client, arg);

        // sleep until callback decrements states_outstanding to 0 or 5 seconds elapse
        for(j=0; (j < 100) && states_outstanding; j++)
        {
            SLEEP_MS(50);
        }
    }
}

/*!
 * \brief trigger publication of relay states by mqtt task
 * 
 * \return nothing
 */
void mqttsu_relay_refresh(void)
{
    // relay states have changed
    mqttsu_queue_send(ALL_RELAYS);
}

/*!
 * \brief wait for timeout or queue
 * 
 * \return true if timeout preempted
 */
int mqttsu_wait(TickType_t timeout)
{
    int err = 0;

    if (xQueueReceive(mqtt_queue, &mqtt_message, timeout) == pdPASS)
    {
        // wait terminated because a message was received
        err = 1;
    }

    return(err);
}

/*!
 * \brief send a message to the mqtt_task queue
 *
 * \param message one byte message
 * 
 * \return nothing
 */
void mqttsu_queue_send(uint8_t message)
{
    static uint8_t message_store = 0;

    if (mqtt_queue_initialized)
    {
        message_store = message;

        // send the message to the queue
        xQueueSend(mqtt_queue, &message_store, 0);
    }
}

/*!
 * \brief initialize a queue for sending messages to the mqtt_task
 * 
 * \return nothing
 */
int mqttsu_initialize_queue(void)
{
    int err = 0;

    // create queue for to pass interrupt messages to task
    mqtt_queue = xQueueCreate(1, sizeof(uint8_t));

    mqtt_queue_initialized = true;

    return(err);
}

/*!
 * \brief safely tear down the mqtt connection
 * 
 * \return nothing
 */
void mqttsu_tear_down(void)
{
    u8_t connection_up = 0;
    int j = 0;

    if (mqtt_client != NULL)
    {
        cyw43_arch_lwip_begin();
        connection_up = mqtt_client_is_connected(mqtt_client);
        cyw43_arch_lwip_end();

        if (connection_up)
        {
            // unsubscribe
            cyw43_arch_lwip_begin();
            mqttsu_end_sub(mqtt_client);
            cyw43_arch_lwip_end(); 

            // disconnect
            disconnect_completed = false;
            cyw43_arch_lwip_begin();
            mqtt_disconnect(mqtt_client);
            cyw43_arch_lwip_end();    
            
            // sleep until disconnect_completed or 5 seconds elapse
            for(j=0; (j < 100) && !disconnect_completed; j++) 
            {
                SLEEP_MS(50);
            }            

        }

        // free client memory
        mqtt_client_free(mqtt_client);
        mqtt_client = NULL;
    }

    // mark connection down
    connection_process_started = false;
    connection_completed = false;
    discovery_completed = false;
    disconnect_completed = false;
    connection_backoff_ms = CONNECTION_BACKOFF_MS_DEFAULT;

    // deinitialize connection related functions (so that they will be rerun)
    mqttsu_deinitialize(mqttsu_initialize_ha_states);
    mqttsu_deinitialize(mqttsu_initialize_ha_discovery);
    mqttsu_deinitialize(mqttsu_initialize_subscription);
    mqttsu_deinitialize(mqttsu_initialize_connection);

    SLEEP_MS(connection_backoff_ms);
}

/*!
 * \brief request connection restart
 * 
 * \return nothing
 */
void mqttsu_request_connection_restart(MQTT_REASON_T reason)
{
    connection_restart_reason = reason;
    connection_restart_request = true;
}

/*!
 * \brief Unsubscribe
 *
 * \param params unused garbage
 * 
 * \return nothing
 */
void mqttsu_end_sub(mqtt_client_t *client)
{
    int err;
    static char topic[32];

    // Unsubscribe
    sprintf(topic, "relay-c-%02x-%02x-%02x-%02x-%02x-%02x/#", web.mac[0], web.mac[1], web.mac[2], web.mac[3], web.mac[4], web.mac[5]); 
    cyw43_arch_lwip_begin();   
    err = mqtt_unsubscribe(client, topic, NULL, NULL);    
    cyw43_arch_lwip_end();

    //printf("unsubscribe result = %d\n", err);
}

  /*!
 * \brief Periodically publish relay states to keep broker connection alive
 * 
 * \return true if timeout preempted
 */
int mqttsu_keep_alive(void)
 {
    int err = 0;
    static uint32_t last_publication = 0;

    if ((unix_time - last_publication) > (60*30))
    {
        if (connection_completed)
        {
            last_publication = unix_time;
            mqttsu_publish_all_relay_states(mqtt_client, NULL); 
        }
    }

    return(err);
 }

 /*!
 * \brief inform mqtt task that the relay configuration changed
 * 
 * \return nothing
 */
void mqttsu_relay_config_change(void)
{
    // trigger republication of discovery
    published_number_of_relays = 0;

    mqttsu_request_connection_restart(MQTT_REASON_CONFIG_CHANGE);
}



 /*!
 * \brief command weather task to begin irrigation of specified zone
 * 
 * \return nothing
 */
void mqttsu_sprinkler_override_begin(int zone)
{
    int i;

    if ((zone >= 0) && (zone < config.zone_max))
    {

        snprintf(web.status_message, sizeof(web.status_message), "MQTT started irrigation of zone %d", zone+1);
        printf("%s\n", web.status_message);  
        mqtt_irrigation_override_zone = zone;    
        set_irrigation_relay_test_zone(zone, -1);
        web.irrigation_override_enable = 1;

        // ensure all other zones are off
        for(i=0; i < config.zone_max; i++)
        {
            if (i != zone)
            {
                web.rmtsw_relay_desired_state[i] = false;
            }
        }

        xTaskNotifyGiveIndexed(worker_tasks[0].task_handle, 0);
    }
}

 /*!
 * \brief command weather task to end irrigation override
 * 
 * \return nothing
 */
void mqttsu_sprinkler_override_end(void)
{
    snprintf(web.status_message, sizeof(web.status_message), "MQTT ended irrigation of zone %d", mqtt_irrigation_override_zone+1);
    printf("%s\n", web.status_message);    
    mqtt_irrigation_override_zone = -1;    
    web.irrigation_override_enable = 0;
    set_irrigation_relay_test_zone(-1, -1); 

    xTaskNotifyGiveIndexed(worker_tasks[0].task_handle, 0);
}


