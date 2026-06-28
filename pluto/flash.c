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

#define BREAKPOINT_FLASH_WRTIE_FAIL (1)   // stop in gdb before cpu reset

extern NON_VOL_VARIABLES_T config;


/*!
 * \brief Copy configuration from flash to RAM
 * 
 * \return 0 on success, -1 on error
 */
int flash_read_non_volatile_variables(CONFIG_TYPE_T config_type)
{
    switch(config_type)
    {
    default:
    case CONFIG_STANDARD:
        memcpy((char *)&config, (char *)(XIP_BASE +  FLASH_TARGET_OFFSET), sizeof(config));
        //flash_dump_config(config_type);
        break;        
    case CONFIG_LEGACY:  // config was originally stored in the last sector of flash -- this now gets overwritten due to RP2350-E10 errata for the Raspberry Pi Pico 2
        memcpy((char *)&config, (char *)(XIP_BASE +  FLASH_LEGACY_OFFSET), sizeof(config));
        //flash_dump_config(config_type);
        break;
    }
    
    return(0);
}


/*!
 * \brief Shim for writing configuration data into flash with interrupts disabled
 * 
 * \param[in]   ptr not used -- for compatibility with flash_safe_execute()
 */
void __no_inline_not_in_flash_func(flash_write_shim)(void *ptr)
{
        // erase the last sector of the flash (4 KBytes)
        flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

        if (sizeof(config) < FLASH_SECTOR_SIZE)
        {
            // program the configuation in 256 Byte pages (range is rounded up to the nearest multiple of 256 Bytes)
            flash_range_program(FLASH_TARGET_OFFSET, (uint8_t *)&config, ((sizeof(config)+255)/256)*256);
        }
        else
        {
            printf("Error: unable to save configuration because it is too large. flash sector size = %d config size = %d\n", FLASH_SECTOR_SIZE, sizeof(config));            
        }
}

/*!
 * \brief Copy configuration from RAM into flash
 * 
 * \return 0 on success
 */
int flash_write_non_volatile_variables(void)
{
    int err = 0;


    if (sizeof(config) < FLASH_SECTOR_SIZE)
    {    
        //vTaskSuspendAll();

        err = flash_safe_execute(flash_write_shim, NULL, 5000);

        if (err)
        {
            printf("flash_safe_execute() returned error %d\n", err);

             #ifdef BREAKPOINT_FLASH_WRTIE_FAIL        
            // Hardcoded breakpoint instruction tells the SWD debugger to freeze right here
            __asm volatile("bkpt #0"); 
            while(1); // Infinite loop prevents the chip from executing further code/resetting
             #endif
        }

        //xTaskResumeAll();
    }
    else
    {
        printf("Error: unable to save configuration because it is too large. flash sector size = %d config size = %d\n", FLASH_SECTOR_SIZE, sizeof(config));        
        err = -2;
    }

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
    printf("Binary start: %08x\nBinary end:   %08x\nBinary size:  %08x\n", start, end, end-start);
    flash_percentage = ((end-start)*1000)/(PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE);
    printf("Flash used:   %d.%d%%\n\n", flash_percentage/10, flash_percentage%10);
}

void flash_get_config_size(void)
{
    int flash_percentage = 0;
    uintptr_t start = (uintptr_t)(FLASH_TARGET_OFFSET);
    uintptr_t end = (uintptr_t)(FLASH_TARGET_OFFSET + FLASH_SECTOR_SIZE - 1);

    if (sizeof(config) > FLASH_SECTOR_SIZE)
    {
        // NB start and end here are with respect to the beginning of flash *not* the cpu address space
        printf("ERROR: Configuration is too large!\n\n");
        printf("Config area start: %08x\nConfig area end:   %08x\nConfig area size:  %08x\n\n", start, end, end-start);
        printf("Config data size:  %d bytes\n", sizeof(config));
        printf("Config data free:  %d bytes\n\n", FLASH_SECTOR_SIZE - sizeof(config));
    }
}

void *flash_get_config_location(CONFIG_TYPE_T config_type)
{
    void *location;

    switch(config_type)
    {
    default:
    case CONFIG_STANDARD:
        location = (void *)(XIP_BASE +  FLASH_TARGET_OFFSET);
        break;        
    case CONFIG_LEGACY:  // config was originally stored in the last sector of flash -- this now gets overwritten due to RP2350-E10 errata for the Raspberry Pi Pico 2
        location = (void *)(XIP_BASE +  FLASH_LEGACY_OFFSET);
        break;
    }

    return(location);
}

/*!
 * \brief Print the contents of flash in hex 
 * 
 * \return 0 on success, 1 on CRC error
 */
int flash_dump_config(CONFIG_TYPE_T config_type)
{
    int i,j;
    char *flash;
    char ascii_output[20];

    printf("Dump Config from Flash %s (%d)\n", config_type==CONFIG_STANDARD?"Standard":"Legacy", config_type);

    flash = (char *)flash_get_config_location(config_type);
    ascii_output[0] = 0;

    for(i=0; i<sizeof(config); i++)
    {
        if ((i & 0x0f) == 0)
        {
            ascii_output[16] = 0;
            printf("%s\n%08x:  ", ascii_output, (flash+i));
        } else if ((i & 0x07) == 0)
        {
            printf(" ");
        }

        printf("%02x ", *(flash+i));

        if (isalnum(*(flash+i)))
        {
            ascii_output[i & 0x0f] = *(flash+i);
        }
        else
        {
            ascii_output[i & 0x0f] = '-';
        }
    }

    if (i & 0x0f)
    {
        for(j=(i & 0x0f); j < 16; j++)
        {
            printf("   ");
            if ((j & 0x07) == 0)
            {
                printf(" ");
            }              
        }        
        ascii_output[(i & 0x0f)] = 0; 
        printf("%s\n", ascii_output); 
    }
    printf("\n");

    return(0);
}
