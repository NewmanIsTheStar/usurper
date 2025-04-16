/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/sockets.h"


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include "config.h"
#include "pluto.h"
#include "utility.h"
#include "config.h"
#include "flash.h"

#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

extern NON_VOL_VARIABLES_T config;


/*!
 * \brief Copy configuration from flash to RAM
 * 
 * \return 0 on success, -1 on error
 */
int flash_read_non_volatile_variables(void)
{
    memcpy((char *)&config, (char *)(XIP_BASE +  FLASH_TARGET_OFFSET), sizeof(config));
    
    return(0);
}

/*!
 * \brief Shim for writing configuration data into flash with interrupts disabled
 * 
 * \param[in]   ptr not used -- for compatibility with flash_safe_execute()
 */
void flash_write_shim(void *ptr)
{
        // erase the last sector of the flash (4 KBytes)
        flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);


        // program the configuation in 256 Byte pages (range is rounded up to the nearest multiple of 256 Bytes)
        flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t *)&config, ((sizeof(config)+255)/256)*256);
}

/*!
 * \brief Copy configuration from RAM into flash
 * 
 * \return 0 on success, -1 on error
 */
int flash_write_non_volatile_variables(void)
{
    int err;

    err = flash_safe_execute(flash_write_shim, NULL, 500);

    return(err);
}

/*!
 * \brief Print the contents of flash in hex 
 * 
 * \return 0 on success, 1 on CRC error
 */
int flash_dump(void)
{
    int i;
    char *flash;

    printf("Dump Flash\n");

    flash = (char *)(XIP_BASE);

    for(i=0; i<2*1024*1024; i++)
    {
        if (i%16 == 0) printf("\n");

        printf("%02x ", *(flash+i));
    }

    return(0);
}


void flash_get_program_size(void)
{
    int flash_percentage = 0;
    extern char __flash_binary_start;  // defined in linker script
    extern char __flash_binary_end;    // defined in linker script

    uintptr_t start = (uintptr_t) &__flash_binary_start;
    uintptr_t end = (uintptr_t) &__flash_binary_end;
    //printf("Binary starts at %08x and ends at %08x, size is %08x\n", start, end, end-start);
    flash_percentage = ((end-start)*1000)/(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE);
    printf("Size: %d.%d%% of flash\n", flash_percentage/10, flash_percentage%10);
}

