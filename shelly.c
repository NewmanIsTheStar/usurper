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
#include "shelly.h"
#include "pluto.h"

//#include "http-client-c.h"

extern WEB_VARIABLES_T web;

// cache of shelly response data
u8_t shelly_ip[128][4];
char shelly_token[128][32];
u8_t shelly_key[255][8];
char shelly_value[255][128];

// prototypes
int initialize_all_keys(u8_t key_value);
int initialize_all_values(char *value_string);
int initialize_all_tokens(void);
int insert_token_string(char *token_string);
int insert_token_value(int token, char *token_value);
int get_free_kvp_row(void);
int append_token_to_key_heirarchy(int kvp_index, u8_t key_token);
int insert_value(int kvp_index, char *value);
int initialize_row_key(int key_index, u8_t key_value);
int append_string_to_key_heirarchy(int kvp_index, char *key_string);
int query_status(char *ipstring);


/*!
 * \brief extract info from shelly json response
 *
 * \param[in]   num_spaces  
 * 
 * \return 0
 */
int parse_shelly_json(char *shelly_json_string)
{
    int depth = 0;
    int comma_instance = 0;
    int i = 0;
    int j = 0;
    int k = 0;
    int l = 0;
    bool consolidate_closing_delimiters = false;
    char instance_delimiter[16] = {'{','{','{','{','{','{','{','{','{','{','{','{','{','{','{','{'};
    int instance[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; 
    char entity[8][32];  // TEMP TESTING
    char last_entity[32];
    bool processing_value[8] = {false, false, false, false, false, false, false, false};
    bool quotation = false;
    bool character_processed = false;
    char key[64];
    char value[64];
    char indexstring[16];
    int kvp_index = 0;


    for(i=0;i<8;i++) sprintf(entity[i], "empty%d", i);
    sprintf(entity[0], "root");
    sprintf(last_entity, "god");
    j = strlen(entity[0]);

    kvp_index = get_free_kvp_row();

    i = 0;
    while (shelly_json_string[i] != 0)
    {
        character_processed = false;

        // track quotes
        if (shelly_json_string[i] == '"')
        {
            quotation = !quotation;
        }

        // process delimiters
        if (!quotation)
        {
            character_processed = true;

            switch (shelly_json_string[i])
            {

            case '{':
            case '[': 
                //entity[depth][j] = 0;        
                depth++;
                instance[depth] = 0;    
                instance_delimiter[depth] = shelly_json_string[i];                          
                processing_value[depth] = false;
                if (consolidate_closing_delimiters)
                {
                    //printf("\n");
                    consolidate_closing_delimiters = false;
                }
                j = 0;
                entity[depth][0] = 0;
                break;
            case '}':
            case ']':  
            case ',':                  
                //printf("[depth = %d][instance = %d]", depth, instance[depth]);
                key[0] = 0;
                initialize_row_key(kvp_index, 255);
                
                for (k=0; k<depth; k++)
                {
                    if(entity[k][0])
                    {
                        //printf("%s", entity[k]);
                        STRAPPEND(key, entity[k]);
                        // token = insert_token_string(entity[k]);
                        // append_token_to_key_heirarchy(kvp_index, token);
                        append_string_to_key_heirarchy(kvp_index, entity[k]);
                    }
                    else
                    {
                        for(l=depth-1; l>0; l--)
                        {
                            if (instance_delimiter[l] == '[') break;
                        }
                        //printf("index%d", instance[l]);
                        sprintf(indexstring, "index%d", instance[l]);
                        STRAPPEND(key, indexstring);
                        // token = insert_token_string(indexstring);
                        // append_token_to_key_heirarchy(kvp_index, token); 
                        append_string_to_key_heirarchy(kvp_index, indexstring);                       
                    }
                    //printf(".");
                    STRAPPEND(key, ".");
                }        
                if (processing_value[depth])
                {
                    //printf("%s (value)", last_entity);
                    STRAPPEND(key, last_entity);
                    // token = insert_token_string(last_entity);
                    // append_token_to_key_heirarchy(kvp_index, token);
                    append_string_to_key_heirarchy(kvp_index, last_entity);

                    //printf("\nKEY: %s\n", key);
                    entity[depth][j] = 0;  
                    //printf("VALUE: %s\n", entity[depth]); 
                    insert_value(kvp_index, entity[depth]);  
                    kvp_index = get_free_kvp_row();                                     
                    processing_value[depth] = false;
                }
                else
                {
                   //printf("*Single token or no token before ] or } or , -- check if array value*\n");
                    if ((instance_delimiter[depth] == '[') && (!consolidate_closing_delimiters))
                    {
                        //printf("instance delimiter is [ so treat as list of values\n");
                        //printf("index%d.", instance[depth]);
                        sprintf(indexstring, "index%d", instance[depth]);
                        STRAPPEND(key, indexstring);  
                        // token = insert_token_string(indexstring);
                        // append_token_to_key_heirarchy(kvp_index, token);
                        append_string_to_key_heirarchy(kvp_index, indexstring); 

                        STRAPPEND(key, ".");

                        //printf("%s (value)", entity[k]);
                        //printf("\nKEY: %s\n", key);
                        entity[depth][j] = 0;  
                        //printf("VALUE: %s\n", entity[depth]);
                        insert_value(kvp_index, entity[depth]);  
                        kvp_index = get_free_kvp_row();                                                 
                    }                    
                }

                if ((shelly_json_string[i] == ']') || (shelly_json_string[i] == '}'))
                {        
                    depth--;                
                    consolidate_closing_delimiters = true;
                    processing_value[depth] = false; 
                } else if (shelly_json_string[i] == ',')
                {
                    //printf("\n");             
                    entity[depth][j] = 0; 
                            
                    instance[depth]++;
                    entity[depth][0] = 0;
                    j = 0;                     
                }
                break;
            case ':':
                //printf("[depth = %d][instance = %d]", depth, instance[depth]);
                for (k=0; k<depth; k++)
                {
                    if(entity[k][0])
                    {
                        //printf("%s", entity[k]);
                    }
                    else
                    {
                        for(l=depth-1; l>0; l--)
                        {
                            if (instance_delimiter[l] == '[') break;
                        }
                        //printf("index%d", instance[l]);
                    }
                    //printf(".");
                }
                entity[depth][j] = 0; 
                strcpy(last_entity, entity[depth]); 
                j = 0;

                if (processing_value[depth])
                {
                    printf("*MISSING VALUE* before :");
                    processing_value[depth] = false;
                }
                else
                {
                    //printf("%s (Parameter)", last_entity); 
                }  
    
                processing_value[depth] = true;            
                
                //printf("\n");                               
                break;                        
            default:
                character_processed = false;
                break;                                
            }
            
        }

        // process standard characters
        if (!character_processed)
        {
            if (consolidate_closing_delimiters)
            {
                //printf("\n");
                consolidate_closing_delimiters = false;
            }   
            if (isprint(shelly_json_string[i]))
            {            
                //printf("%c", shelly_json_string[i]);
                entity[depth][j++] = shelly_json_string[i];
            }
        }

        i++;
    }

    return(0);
}



/*!
 * \brief set all keys 
 *
 * \param[in]   key_value value to assign to each key  
 * 
 * \return 0 on success
 */
int initialize_all_keys(u8_t key_value)
{
    int key_index;
    int key_heirarchy;

    for(key_index = 0; key_index < NUM_ROWS(shelly_key); key_index++)
    {
        for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(shelly_key[0]); key_heirarchy++)
        {
            shelly_key[key_index][key_heirarchy] = key_value;
        }    
    }

    return(0);
}

/*!
 * \brief set all keys 
 *
 * \param[in]   key_value value to assign to each key  
 * 
 * \return 0 on success, -1 on error
 */
int initialize_row_key(int key_index, u8_t key_value)
{
    int err = 0;
    int key_heirarchy;

    if ((key_index >0) && (key_index < NUM_ROWS(shelly_key)))
    {
        for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(shelly_key[0]); key_heirarchy++)
        {
            shelly_key[key_index][key_heirarchy] = key_value;
        }    
    }
    else
    {
        err = -1;
    }

    return(err);
}

/*!
 * \brief extract info from shelly json response
 *
 * \param[in]   value_string value to assign to each value   
 * 
 * \return 0 on success
 */
int initialize_all_values(char *value_string)
{
    int value_index;

    for(value_index = 0; value_index < NUM_ROWS(shelly_value); value_index++)
    {
        if (value_string && (value_string[0] != 0))
        {
            STRNCPY(shelly_value[value_index], value_string, sizeof(shelly_value[0]));
        }
        else
        {
           shelly_value[value_index][0] = 0; 
        }
    }

    return(0);
}

/*!
 * \brief set all tokens to empty string 
 * 
 * \return 0 on success
 */
int initialize_all_tokens(void)
{
    int token_index;

    for(token_index = 0; token_index < NUM_ROWS(shelly_token); token_index++)
    {
        shelly_token[token_index][0] = 0; 
    }

    return(0);
}

/*!
 * \brief add string to token cache and return token
 *
 * \param[in]   token_string string to tokenize  
 * 
 * \return token or -1 on error
 */
int insert_token_string(char *token_string)
{
    int token_index;

    for(token_index = 0; token_index < NUM_ROWS(shelly_token); token_index++)
    {
        if (strcasecmp(token_string, shelly_token[token_index]) == 0)
        {
            // found existing entry
            break;
        }
        else if (shelly_token[token_index][0] == 0)
        {
            // found unused token so assign
            STRNCPY(shelly_token[token_index], token_string, sizeof(shelly_token[0]));
            break;
        }
    }

    if (token_index >= NUM_ROWS(shelly_token))
    {
        // out of space in token table
        token_index = -1;
        printf("***********************************************OUT OF TOKENS!");
    }

    return(token_index);
}


/*!
 * \brief get an index to an empty kvp cache row
 * 
 * \return key-value-pair index on success, -1 on error
 */
int get_free_kvp_row(void)
{
    int kvp_index = 0;
    
    for(kvp_index = 0; kvp_index < NUM_ROWS(shelly_key); kvp_index++)
    {
        if (shelly_key[kvp_index][0] == 255)
        {
            // found a free row
            break;
        } 
    }

    if (kvp_index >= NUM_ROWS(shelly_key))
    {
        kvp_index = -1;
    }

    return(kvp_index);
}

/*!
 * \brief add token to heirarchy for given kvp cache row
 * 
 * \return 0 on success, -1 on error
 */
int append_token_to_key_heirarchy(int kvp_index, u8_t key_token)
{
    int err = -1;
    int key_heirarchy = 0;
    
    if ((kvp_index >=0) && (kvp_index < NUM_ROWS(shelly_key)))
    {

        for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(shelly_key[0]); key_heirarchy++)
        {
            if (shelly_key[kvp_index][key_heirarchy] == 255)
            {
                // found space to append to key heirarchy in this row
                shelly_key[kvp_index][key_heirarchy] = key_token;
                err = 0;
                break;
            }
        }           
        
    }

    return(err);
}

/*!
 * \brief add token to heirarchy for given kvp cache row
 * 
 * \return 0 on success, -1 on error
 */
int append_string_to_key_heirarchy(int kvp_index, char *key_string)
{
    int err = -1;
    int key_heirarchy = 0;
    int token = 0;

   
    if ((kvp_index >=0) && (kvp_index < NUM_ROWS(shelly_key)))
    {
        // tokenize and append
        token = insert_token_string(key_string);
        err = append_token_to_key_heirarchy(kvp_index, token);             
    }

    return(err);
}



/*!
 * \brief add value to cache
 *
 * \param[in]   kvp_index key-value-pair index 
 * 
 * \return 0 on success, -1 on error
 */
int insert_value(int kvp_index, char *value)
{
    int err = 0;

    if ((kvp_index >= 0) && (kvp_index < NUM_ROWS(shelly_value)))
    {
        STRNCPY(shelly_value[kvp_index], value, sizeof(shelly_value[0]));
    }

    return(err);
}

/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int initialize_shelly_cache(void)
{
    initialize_all_keys(255);
    initialize_all_values("");
    initialize_all_tokens();    

    return(0);
}

/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int dump_shelly_cache(void)
{
    int key_index;
    int key_heirarchy;

    for(key_index = 0; key_index < NUM_ROWS(shelly_key); key_index++)
    {
        if (shelly_key[key_index][0] != 255)
        {
            printf("KEY_%03d = ", key_index);
            for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(shelly_key[0]); key_heirarchy++)
            {
                if (shelly_key[key_index][key_heirarchy] != 255)
                {
                    if (key_heirarchy > 0)
                    {
                        printf(".%s", shelly_token[shelly_key[key_index][key_heirarchy]]);
                    }
                    else
                    {
                        printf("%s", shelly_token[shelly_key[key_index][key_heirarchy]]);
                    }
                }
            } 

            printf(" VALUE =");
            printf("%s\n", shelly_value[key_index]);
        }
    }

    return(0);
}

/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int dump_shelly_tokens(void)
{
    int token_index;

    for(token_index = 0; token_index < NUM_ROWS(shelly_token); token_index++)
    {
        if (shelly_token[token_index][0] != 0)
        {
            printf("TOKEN_%03d = %s\n", token_index, shelly_token[token_index]);

        } 
    }

    return(0);
}


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

    printf("search range: %08x to %08x\n", search_start, search_end);

    for(ip_to_query=search_start+1; ip_to_query<search_end; ip_to_query++)
    {
        //byte = *((u8_t *)&ip_to_query);
        //printf("byte = %0x\n", byte);
        sprintf(ipstring, "%d.%d.%d.%d", ((u8_t *)&ip_to_query)[3], ((u8_t *)&ip_to_query)[2], ((u8_t *)&ip_to_query)[1], ((u8_t *)&ip_to_query)[0]);

        printf("querying %s\n", ipstring);
        query_status(ipstring);

        sleep_ms(10000);
    }

    return(0);
}

/*!
 * \brief Testing http-client-c from github TODO: get the memory leak fixes waiting in a merge request
 *
 * \param[in]   on 1=on, 0=off  
 * 
 * \return 0 on success, non-zero on error
 */
int query_status(char *ipstring)  
{
    char url[128];
    // struct http_response *hresp;

    // sprintf(url, "http://%s/status", ipstring);
    // printf("url = %s\n", url);

    // hresp = http_get(url, "User-agent:MyUserAgent\r\n");
    

    // printf("status code string = ");
    // print_printable_text(hresp->status_code);
    // printf("status code int = %d\n", hresp->status_code_int);
    // printf("status text = ");
    // print_printable_text(hresp->status_text);
    

    // printf("body = \n");
    // print_printable_text(hresp->body);

    // parse_shelly_json(hresp->body);

    return(0);
}

/*!
 * \brief Retrieve the value matching a given key from the json cache
 * 
 * \param[in]   key             key to find
 * 
 * \param[out]  value           value found
 * 
 * \param[in]   value_length    max length of value
 * 
 * \param[in]   strip_quotes    remove outer quotes from value (if present)
 *     
 * \return 0 on success, -1 on error
 */
int json_get_value(const char *key, char *value, int value_length, bool strip_quotes)
{
    int key_index;
    int key_heirarchy;
    char search_key[256];
    int err = 1;

    for(key_index = 0; key_index < NUM_ROWS(shelly_key); key_index++)
    {
        if (shelly_key[key_index][0] != 255)
        {
            // begin constructing new search key
            search_key[0] = 0;

            //printf("KEY_%03d = ", key_index);
            for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(shelly_key[0]); key_heirarchy++)
            {
                if (shelly_key[key_index][key_heirarchy] != 255)
                {
                    if (key_heirarchy > 0)
                    {                        
                        STRAPPEND(search_key, ".");
                        //printf(".%s", shelly_token[shelly_key[key_index][key_heirarchy]]);
                    }
                    else
                    {
                        //printf("%s", shelly_token[shelly_key[key_index][key_heirarchy]]);
                    }
                    STRAPPEND(search_key, shelly_token[shelly_key[key_index][key_heirarchy]]);
                }
            } 

            //printf(" VALUE =");
            //printf("%s\n", shelly_value[key_index]);

            if (!strncasecmp(search_key, key, strlen(key)))
            {
                //printf("***MATCH****\n");
                STRNCPY(value, shelly_value[key_index], value_length);  //TODO copy with outer quote removal
                err = 0;
            }
        }
    }

    return(err);
}