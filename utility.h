
/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef UTILITY_H
#define UTILITY_H

// standard macros
#define STRNCPY(dst, src, size)  {strncpy((dst), (src), (size)); *((dst)+(size)-1)=0;}
#define STRNCAT(dst, src, size)  {if ((size) > 0) {*((dst)+(size)-1)=0; strncat((dst), (src), (size)-strlen((dst))-1);};}
// WARNING STRCAT macro can only be used when the destination size can be found with sizeof() if a pointer is the destination then use STRNCAT
#define STRCAT(dst, src)  {*((dst)+(sizeof(dst))-1)=0; strncat((dst), (src), (sizeof(dst))-strlen((dst))-1);}
#define NUM_ROWS(x) (sizeof(x)/sizeof(x[0]))
#define MASKED_WRITE(dest,src,mask) {(dest) = (((dest) & (~(mask))) | ((src) & (mask)));}
#define CLIP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))


typedef struct
{
    int lock;
    int write_data;
    int read_data;
} DOUBLE_BUF_INT;

uint16_t crc_buffer(char *pbuff, int num_bytes);
void hex_dump_to_string(const uint8_t *bptr, uint32_t len, char *out_string, int out_len);
void hex_dump(const uint8_t *bptr, uint32_t len);
int establish_socket(char *address_string, struct sockaddr_in *ipv4_address, int port, int type);
int send_syslog_message(char *log_name, const char *format, ...);
int check_watchdog_reboot(void);
int send_govee_command(int on, int red, int green, int blue);
int establish_multicast_socket(struct sockaddr_in *ipv4_address, int port, int type);
int JoinGroup(int sock, const char* join_ip, const char* local_ip);
int send_pluto_message(char *message);
int set_double_buf_integer(DOUBLE_BUF_INT *integer, int value);
int get_double_buf_integer(DOUBLE_BUF_INT *integer, int retry);
int initialize_relay_gpio_(int gpio_number);
int deplus_string(char *string, int max_len);
int send_shelly_command(int on);
int test_http(int on);
int print_printable_text(char *contaminated_string);
int indent(int num_spaces);  

#endif