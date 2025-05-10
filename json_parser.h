#ifndef JSON_PARSER_H
#define JSON_PARSER_H

typedef struct
{
    int depth;
    int comma_instance;
    int i;
    int j;
    int k;
    int l;
    bool consolidate_closing_delimiters;
    char instance_delimiter[16];
    int instance[16]; 
    char entity[8][32];  // TEMP TESTING
    char last_entity[32];
    bool processing_value[8];
    bool quotation;
    bool character_processed;
    char key[64];
    char value[64];
    char indexstring[16];
    int kvp_index;
} JSON_PARSER_CONTEXT_T;

int jsonp_initialize_context(JSON_PARSER_CONTEXT_T *context);
int jsonp_initialize_cache(void);
int jsonp_parse_buffer(JSON_PARSER_CONTEXT_T *context, char *buffer, bool continuation);
int jsonp_dump_key_value_pairs(void);
int jsonp_dump_tokens(void);
int jsonp_discover_shelly_devices(void);
int jsonp_get_value(const char *key, char *value, int value_length, bool strip_quotes);

#endif
