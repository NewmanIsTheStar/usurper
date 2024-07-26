#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include <hardware/flash.h>

#include "lwip/sockets.h"


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

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
    NON_VOL_VARIABLES_T *non_vol;
    int i;
    int len;
    uint16_t crc;
    int write_to_flash;

    // compute ram crc and store in ram copy of configuration
    len = offsetof(struct NON_VOL_VARIABLES, crc);
    config.crc = crc_buffer((uint8_t *)&config, len);

    // compute flash crc
    non_vol = (NON_VOL_VARIABLES_T *)(XIP_BASE +  FLASH_TARGET_OFFSET);
    crc = crc_buffer((uint8_t *)non_vol, len);

    if (config.crc != crc)
    {
        write_to_flash = 1;
    }
    else
    {
        write_to_flash = 0;

        // crc matches, 1/65536 random chance of a false match so check byte by byte
        for (i=0; i<sizeof(config); i++)
        {
            if (((char *)(XIP_BASE +  FLASH_TARGET_OFFSET))[i] != ((char *)&config)[i])
            {
                printf("Found byte difference at offset %d so will write flash even though CRC matches\n", i);
                write_to_flash = 1;
                break;
            }
        }
    }

    if (write_to_flash)
    {
        printf("Writing non-vol variables to flash.  CRC = %d\n", non_vol->crc);

        i = flash_safe_execute(flash_write_shim, NULL, 500);

        printf("Finished writing flash with status %d\n", i);        
    }
    else
    {
        printf("Refusing to write to flash since no changes detected\n");
    }



    return(0);
}

/*!
 * \brief Check if the CRC stored in the flash configuration is correct 
 * 
 * \return 0 on success, 1 on CRC error
 */
int flash_corrupt(void)
{
    NON_VOL_VARIABLES_T *non_vol;
    int len;
    uint16_t crc;
    int err;

    non_vol = (NON_VOL_VARIABLES_T *)(XIP_BASE +  FLASH_TARGET_OFFSET);

    len = offsetof(struct NON_VOL_VARIABLES, crc);
    
    crc = crc_buffer((uint8_t *)non_vol, len);

    if (crc != non_vol->crc)
    {
        printf("Flash Configuration Parameters: calculated crc = %d  vs. stored crc = %d\n", crc, non_vol->crc);
    }

    return(crc != non_vol->crc);
}

/*!
 * \brief Write default configuration values to flash 
 * 
 * \return 0 on success, 1 on CRC error
 */
int flash_initialize_non_volatile_variables(void)
{
    int i;

    // zero the configuration
    memset((void *)&config, 0, sizeof(config));

    config_initialize();
    
    flash_write_non_volatile_variables();

    return(0);
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
    int len;
    uint16_t crc;
    int err;

    printf("Dump Flash\n");

    flash = (char *)(XIP_BASE);

    for(i=0; i<2*1024*1024; i++)
    {
        if (i%16 == 0) printf("\n");

        printf("%02x ", *(flash+i));
    }

    return(0);
}




