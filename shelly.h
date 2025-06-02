#ifndef SHELLY_H
#define SHELLY_H

#include "powerwall.h"

int discover_shelly_devices(void);
int shelly_http_request(HTTP_REQUEST_TYPE_T type, char *url, char *host, char *content);

#endif
