
#ifndef MESSAGE_H
#define MESSAGE_H

#include "udp.h"
#include "message_defs.h"

#define SOCKADDR_LEN sizeof(struct sockaddr)

//typedef int SOCKET;
//typedef struct sockaddr SOCKADDR;
//typedef struct sockaddr_in SOCKADDR_IN;

void message_task(__unused void *params);
//int sock_StartUdp (int iPort);
////int sock_Transmit (char *pbBuffer, int iLength, SOCKADDR_IN sDest);
//char *sock_Receive (int *iRxLength, SOCKADDR_IN *psFromAddress);
void send_test_message(void);
int check_received_header(tsMSG_HDR *psMsg, SOCKADDR_IN sDest);
void set_led_pattern_remote(int pattern); 
void set_led_speed_remote(int speed); 

#endif