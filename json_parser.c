#include <pico/cyw43_arch.h>
#include <pico/stdlib.h>
#include <pico/util/datetime.h>
//#include "hardware/rtc.h"
#include <hardware/watchdog.h>
#include <hardware/flash.h>

#include <lwip/netif.h>
#include <lwip/ip4_addr.h>
#include <lwip/apps/lwiperf.h>
#include <lwip/opt.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/sys.h>
#include <lwip/dns.h>

#include <FreeRTOS.h>
#include <FreeRTOSConfig.h>
#include <task.h>


#include <stdarg.h>

#include <weather.h>
#include <flash.h>
#include <calendar.h>
#include <utility.h>
#include <config.h>
#include <watchdog.h>
#include <pluto.h>
#include "powerwall.h"
#include <shelly.h>
#include "json_parser.h"


extern WEB_VARIABLES_T web;


// prototypes
int jsonp_initialize_all_keys(JSON_PARSER_CONTEXT_T *context, u8_t key_value);
int jsonp_initialize_all_values(JSON_PARSER_CONTEXT_T *context, char *value_string);
int jsonp_initialize_all_tokens(JSON_PARSER_CONTEXT_T *context);
int jsonp_insert_token_string(JSON_PARSER_CONTEXT_T *context, char *token_string);
int jsonp_insert_token_value(JSON_PARSER_CONTEXT_T *context, int token, char *token_value);
int jsonp_get_free_kvp_row(JSON_PARSER_CONTEXT_T *context);
int jsonp_append_token_to_key_heirarchy(JSON_PARSER_CONTEXT_T *context, int kvp_index, u8_t key_token);
int jsonp_insert_value(JSON_PARSER_CONTEXT_T *context, int kvp_index, char *value);
int jsonp_initialize_row_key(JSON_PARSER_CONTEXT_T *context, int key_index, u8_t key_value);
int jsonp_append_string_to_key_heirarchy(JSON_PARSER_CONTEXT_T *context, int kvp_index, char *key_string);
int jsonp_query_status(char *ipstring);




/*!
 * \brief initialize json parser context
 *
 * \param[in]   num_spaces  
 * 
 * \return 0
 */
int jsonp_initialize_context(JSON_PARSER_CONTEXT_T *context)
{
    int i;

    context->depth = 0;
    context->comma_instance = 0;
    context->i = 0;
    context->j = 0;
    context->k = 0;
    context->l = 0;
    context->consolidate_closing_delimiters = false;
    context->last_entity[0] = 0;
    context->quotation = false;
    context->character_processed = false;
    context->kvp_index = 0; 

    memset(&(context->instance_delimiter), '{', NUM_ROWS(context->instance_delimiter));
    memset(&(context->instance), 0, NUM_ROWS(context->instance));
    memset(&(context->entity), 0, sizeof(context->entity));
    memset(&(context->processing_value), false, NUM_ROWS(context->processing_value));
    memset(&(context->key), 0, NUM_ROWS(context->key));
    memset(&(context->value), 0, NUM_ROWS(context->value));
    memset(&(context->indexstring), 0, NUM_ROWS(context->indexstring));

    // TODO -- not needed here as we have specific cache init function
    // memset(&(context->jsonp_token), 0, sizeof(context->jsonp_token));
    // memset(&(context->jsonp_key), 0, sizeof(context->jsonp_key));
    // memset(&(context->jsonp_value), 0, sizeof(context->jsonp_value));

    for(i=0; i<8; i++)
    {
        sprintf(context->entity[i], "empty%d", i);
    } 

    sprintf(context->entity[0], "root");
    sprintf(context->last_entity, "god"); 

    return(0);
}


/*!
 * \brief extract key value pairs from buffer containing json
 *
 * \param[in]   num_spaces  
 * 
 * \return 0
 */
int jsonp_parse_buffer(JSON_PARSER_CONTEXT_T *context, char *buffer, bool continuation)
{
    // reset parser unless buffer continues from the previous buffer
    if (!continuation)
    {
        jsonp_initialize_context(context); 
        jsonp_initialize_cache(context);       
    }

    context->j = strlen(context->entity[0]);
    context->kvp_index = jsonp_get_free_kvp_row(context);
    context->i = 0;

    while (buffer[context->i] != 0)
    {
        context->character_processed = false;

        // track quotes
        if (buffer[context->i] == '"')
        {
            context->quotation = !context->quotation;
        }

        // process delimiters
        if (!context->quotation)
        {
            context->character_processed = true;

            switch (buffer[context->i])
            {

            case '{':
            case '[':        
                context->depth++;       
                if (context->depth >= NUM_ROWS(context->instance))
                {
                    printf("Parser ABORT -- maximum nesting depth exceeded\n");
                    break;
                }         
                context->instance[context->depth] = 0;    
                context->instance_delimiter[context->depth] = buffer[context->i];                          
                context->processing_value[context->depth] = false;
                if (context->consolidate_closing_delimiters)
                {
                    //printf("\n");
                    context->consolidate_closing_delimiters = false;
                }
                context->j = 0;
                context->entity[context->depth][0] = 0;
                break;
            case '}':
            case ']':  
            case ',':                  
                //printf("[depth = %d][instance = %d]", depth, instance[depth]);
                context->key[0] = 0;
                jsonp_initialize_row_key(context, context->kvp_index, 255);
                
                for (context->k=0; context->k<context->depth; context->k++)
                {
                    if(context->entity[context->k][0])
                    {
                        //printf("%s", entity[k]);
                        STRAPPEND(context->key, context->entity[context->k]);
                        jsonp_append_string_to_key_heirarchy(context, context->kvp_index, context->entity[context->k]);
                    }
                    else
                    {
                        for(context->l=context->depth-1; context->l>0; context->l--)
                        {
                            if (context->instance_delimiter[context->l] == '[') break;
                        }
                        //printf("index%d", instance[l]);
                        sprintf(context->indexstring, "index%d", context->instance[context->l]);
                        STRAPPEND(context->key, context->indexstring);
                        jsonp_append_string_to_key_heirarchy(context, context->kvp_index, context->indexstring);                       
                    }
                    //printf(".");
                    STRAPPEND(context->key, ".");
                }        
                if (context->processing_value[context->depth])
                {
                    //printf("%s (value)", last_entity);
                    STRAPPEND(context->key, context->last_entity);
                    jsonp_append_string_to_key_heirarchy(context, context->kvp_index, context->last_entity);

                    //printf("\nKEY: %s\n", key);
                    context->entity[context->depth][context->j] = 0;  
                    //printf("VALUE: %s\n", entity[depth]); 
                    jsonp_insert_value(context, context->kvp_index, context->entity[context->depth]);  
                    context->kvp_index = jsonp_get_free_kvp_row(context);                                     
                    context->processing_value[context->depth] = false;
                }
                else
                {
                   //printf("*Single token or no token before ] or } or , -- check if array value*\n");
                    if ((context->instance_delimiter[context->depth] == '[') && (!context->consolidate_closing_delimiters))
                    {
                        //printf("instance delimiter is [ so treat as list of values\n");
                        //printf("index%d.", instance[depth]);
                        sprintf(context->indexstring, "index%d", context->instance[context->depth]);
                        STRAPPEND(context->key, context->indexstring);  
                        jsonp_append_string_to_key_heirarchy(context, context->kvp_index, context->indexstring); 

                        STRAPPEND(context->key, ".");

                        //printf("%s (value)", entity[k]);
                        //printf("\nKEY: %s\n", key);
                        context->entity[context->depth][context->j] = 0;  
                        //printf("VALUE: %s\n", entity[depth]);
                        jsonp_insert_value(context, context->kvp_index, context->entity[context->depth]);  
                        context->kvp_index = jsonp_get_free_kvp_row(context);                                                 
                    }                    
                }

                if ((buffer[context->i] == ']') || (buffer[context->i] == '}'))
                {        
                    context->depth--;                
                    context->consolidate_closing_delimiters = true;
                    context->processing_value[context->depth] = false; 
                } else if (buffer[context->i] == ',')
                {
                    //printf("\n");             
                    context->entity[context->depth][context->j] = 0; 
                            
                    context->instance[context->depth]++;
                    context->entity[context->depth][0] = 0;
                    context->j = 0;                     
                }
                break;
            case ':':
                //printf("[depth = %d][instance = %d]", depth, instance[depth]);
                for (context->k=0; context->k<context->depth; context->k++)
                {
                    if(context->entity[context->k][0])
                    {
                        //printf("%s", entity[k]);
                    }
                    else
                    {
                        for(context->l=context->depth-1; context->l>0; context->l--)
                        {
                            if (context->instance_delimiter[context->l] == '[') break;
                        }
                        //printf("index%d", instance[l]);
                    }
                    //printf(".");
                }
                context->entity[context->depth][context->j] = 0; 
                strcpy(context->last_entity, context->entity[context->depth]); 
                context->j = 0;

                if (context->processing_value[context->depth])
                {
                    printf("*MISSING VALUE* before :");
                    context->processing_value[context->depth] = false;
                }
                else
                {
                    //printf("%s (Parameter)", last_entity); 
                }  
    
                context->processing_value[context->depth] = true;            
                
                //printf("\n");                               
                break;                        
            default:
            context->character_processed = false;
                break;                                
            }
            
        }

        // process standard characters
        if (!context->character_processed)
        {
            if (context->consolidate_closing_delimiters)
            {
                //printf("\n");
                context->consolidate_closing_delimiters = false;
            }   
            if (isprint(buffer[context->i]))
            {            
                //printf("%c", shelly_json_string[i]);
                context->entity[context->depth][context->j++] = buffer[context->i];
            }
        }

        context->i++;
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
int jsonp_initialize_all_keys(JSON_PARSER_CONTEXT_T *context, u8_t key_value)
{
    int key_index;
    int key_heirarchy;

    for(key_index = 0; key_index < NUM_ROWS(context->jsonp_key); key_index++)
    {
        for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(context->jsonp_key[0]); key_heirarchy++)
        {
            context->jsonp_key[key_index][key_heirarchy] = key_value;
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
int jsonp_initialize_row_key(JSON_PARSER_CONTEXT_T *context, int key_index, u8_t key_value)
{
    int err = 0;
    int key_heirarchy;

    if ((key_index >0) && (key_index < NUM_ROWS(context->jsonp_key)))
    {
        for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(context->jsonp_key[0]); key_heirarchy++)
        {
            context->jsonp_key[key_index][key_heirarchy] = key_value;
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
int jsonp_initialize_all_values(JSON_PARSER_CONTEXT_T *context, char *value_string)
{
    int value_index;

    for(value_index = 0; value_index < NUM_ROWS(context->jsonp_value); value_index++)
    {
        if (value_string && (value_string[0] != 0))
        {
            STRNCPY(context->jsonp_value[value_index], value_string, sizeof(context->jsonp_value[0]));
        }
        else
        {
           context->jsonp_value[value_index][0] = 0; 
        }
    }

    return(0);
}

/*!
 * \brief set all tokens to empty string 
 * 
 * \return 0 on success
 */
int jsonp_initialize_all_tokens(JSON_PARSER_CONTEXT_T *context)
{
    int token_index;

    for(token_index = 0; token_index < NUM_ROWS(context->jsonp_token); token_index++)
    {
        context->jsonp_token[token_index][0] = 0; 
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
int jsonp_insert_token_string(JSON_PARSER_CONTEXT_T *context, char *token_string)
{
    int token_index;

    for(token_index = 0; token_index < NUM_ROWS(context->jsonp_token); token_index++)
    {
        if (strcasecmp(token_string, context->jsonp_token[token_index]) == 0)
        {
            // found existing entry
            break;
        }
        else if (context->jsonp_token[token_index][0] == 0)
        {
            // found unused token so assign
            STRNCPY(context->jsonp_token[token_index], token_string, sizeof(context->jsonp_token[0]));
            break;
        }
    }

    if (token_index >= NUM_ROWS(context->jsonp_token))
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
int jsonp_get_free_kvp_row(JSON_PARSER_CONTEXT_T *context)
{
    int kvp_index = 0;
    
    for(kvp_index = 0; kvp_index < NUM_ROWS(context->jsonp_key); kvp_index++)
    {
        if (context->jsonp_key[kvp_index][0] == 255)
        {
            // found a free row
            break;
        } 
    }

    if (kvp_index >= NUM_ROWS(context->jsonp_key))
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
int jsonp_append_token_to_key_heirarchy(JSON_PARSER_CONTEXT_T *context, int kvp_index, u8_t key_token)
{
    int err = -1;
    int key_heirarchy = 0;
    
    if ((kvp_index >=0) && (kvp_index < NUM_ROWS(context->jsonp_key)))
    {

        for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(context->jsonp_key[0]); key_heirarchy++)
        {
            if (context->jsonp_key[kvp_index][key_heirarchy] == 255)
            {
                // found space to append to key heirarchy in this row
                context->jsonp_key[kvp_index][key_heirarchy] = key_token;
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
int jsonp_append_string_to_key_heirarchy(JSON_PARSER_CONTEXT_T *context, int kvp_index, char *key_string)
{
    int err = -1;
    int key_heirarchy = 0;
    int token = 0;

   
    if ((kvp_index >=0) && (kvp_index < NUM_ROWS(context->jsonp_key)))
    {
        // tokenize and append
        token = jsonp_insert_token_string(context, key_string);
        err = jsonp_append_token_to_key_heirarchy(context, kvp_index, token);             
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
int jsonp_insert_value(JSON_PARSER_CONTEXT_T *context, int kvp_index, char *value)
{
    int err = 0;

    if ((kvp_index >= 0) && (kvp_index < NUM_ROWS(context->jsonp_value)))
    {
        STRNCPY(context->jsonp_value[kvp_index], value, sizeof(context->jsonp_value[0]));
    }

    return(err);
}

/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int jsonp_initialize_cache(JSON_PARSER_CONTEXT_T *context)
{
    jsonp_initialize_all_keys(context, 255);
    jsonp_initialize_all_values(context, "");
    jsonp_initialize_all_tokens(context);    

    return(0);
}

/*!
 * \brief print cached key value pairs
 * 
 * \return 0 on success, -1 on error
 */
int jsonp_dump_key_value_pairs(JSON_PARSER_CONTEXT_T *context)
{
    int key_index;
    int key_heirarchy;

    for(key_index = 0; key_index < NUM_ROWS(context->jsonp_key); key_index++)
    {
        if (context->jsonp_key[key_index][0] != 255)
        {
            printf("KEY_%03d = ", key_index);
            for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(context->jsonp_key[0]); key_heirarchy++)
            {
                if (context->jsonp_key[key_index][key_heirarchy] != 255)
                {
                    if (key_heirarchy > 0)
                    {
                        printf(".%s", context->jsonp_token[context->jsonp_key[key_index][key_heirarchy]]);
                    }
                    else
                    {
                        printf("%s", context->jsonp_token[context->jsonp_key[key_index][key_heirarchy]]);
                    }
                }
            } 

            printf(" VALUE =");
            printf("%s\n", context->jsonp_value[key_index]);
        }
    }

    return(0);
}

/*!
 * \brief clear cache used to store shelly device responses
 * 
 * \return 0 on success, -1 on error
 */
int jsonp_dump_tokens(JSON_PARSER_CONTEXT_T *context)
{
    int token_index;

    for(token_index = 0; token_index < NUM_ROWS(context->jsonp_token); token_index++)
    {
        if (context->jsonp_token[token_index][0] != 0)
        {
            printf("TOKEN_%03d = %s\n", token_index, context->jsonp_token[token_index]);

        } 
    }

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
int jsonp_get_value(JSON_PARSER_CONTEXT_T *context, const char *key, char *value, int value_length, bool strip_quotes)
{
    int key_index;
    int key_heirarchy;
    char search_key[256];
    int err = 1;

    for(key_index = 0; key_index < NUM_ROWS(context->jsonp_key); key_index++)
    {
        if (context->jsonp_key[key_index][0] != 255)
        {
            // begin constructing new search key
            search_key[0] = 0;

            //printf("KEY_%03d = ", key_index);
            for(key_heirarchy = 0; key_heirarchy < NUM_ROWS(context->jsonp_key[0]); key_heirarchy++)
            {
                if (context->jsonp_key[key_index][key_heirarchy] != 255)
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
                    STRAPPEND(search_key, context->jsonp_token[context->jsonp_key[key_index][key_heirarchy]]);
                }
            } 

            //printf(" VALUE =");
            //printf("%s\n", shelly_value[key_index]);

            if (!strncasecmp(search_key, key, strlen(key)))
            {
                //printf("***MATCH****\n");
                STRNCPY(value, context->jsonp_value[key_index], value_length);  //TODO copy with outer quote removal
                err = 0;
            }
        }
    }

    return(err);
}