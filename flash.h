#ifndef FLASH_H
#define FLASH_H

int flash_read_non_volatile_variables(void);
int flash_write_non_volatile_variables(void);
int flash_corrupt(void);
uint16_t crc_buffer(char *pbuff, int num_bytes);
int flash_initialize_non_volatile_variables(void);
int flash_dump(void);



#endif