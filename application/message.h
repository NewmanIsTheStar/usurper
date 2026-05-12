
#ifndef MESSAGE_H
#define MESSAGE_H

#include "udp.h"
#include "message_defs.h"

#define SOCKADDR_LEN sizeof(struct sockaddr)

void message_task(__unused void *params);
void send_test_message(void);
int check_received_header(tsMSG_HDR *psMsg, SOCKADDR_IN sDest);
void set_led_pattern_remote(int pattern); 
void set_led_speed_remote(int speed); 

#endif