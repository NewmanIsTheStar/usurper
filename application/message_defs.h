#ifndef MESSAGE_DEFS_H
#define MESSAGE_DEFS_H

#include <limits.h>

// udp message identifiers
typedef enum
{
    LED_STRIP_RQST             =   0,  // client to server
    LED_STRIP_CNFM             =   1,  // server to client
    WIND_SPEED_RQST            =   2,  // client to server
    WIND_SPEED_CNFM            =   3,  // server to client    
    
    NO_MSG                     =  4294967295,   //INT_MAX not sufficient 
} teMSG_ID;

// message header
typedef struct
{
    uint32_t version;
    teMSG_ID message;
    uint32_t transaction;
    uint32_t sequence;
} tsMSG_HDR;

typedef struct
{
    tsMSG_HDR sHeader;
    int pattern;
    int speed;
} tsLED_STRIP_RQST;

typedef struct
{
    tsMSG_HDR sHeader;
    int iError;      // 0 = no error, 1 = error
} tsLED_STRIP_CNFM;

typedef struct
{
    tsMSG_HDR sHeader;
} tsWIND_SPEED_RQST;

typedef struct
{
    tsMSG_HDR sHeader;
    int iError;
    int wind_speed;
} tsWIND_SPEED_CNFM;

#endif

