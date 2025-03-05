#ifndef SHELLY_H
#define SHELLY_H

int initialize_shelly_cache(void);
int parse_shelly_json(char *shelly_json_string);
int dump_shelly_cache(void);
int dump_shelly_tokens(void);
int discover_shelly_devices(void);
int json_get_value(const char *key, char *value, int value_length, bool strip_quotes);

#endif
