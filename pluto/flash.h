/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef FLASH_H
#define FLASH_H

/*Explanation
  The configuration was originally stored in the last sector of flash.
  The Pi Pico2 had a hardware bug that needed a work around.
  That workaround overwrites the last sector of flash when dragging 
  and dropping the UF2 file onto the Pi Pico2 -- thus nuking the config.
  Therefore the config has been moved to the penultimate flash sector.
  See "RP2350-E10 errata for the Raspberry Pi Pico 2" for more info.
*/
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - (2*FLASH_SECTOR_SIZE))
#define FLASH_LEGACY_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

typedef enum
{
    CONFIG_STANDARD     = 0,
    CONFIG_LEGACY       = 1,

    NUM_CONFIG_TYPES    = 3
} CONFIG_TYPE_T;

int flash_read_non_volatile_variables(CONFIG_TYPE_T config_type);
int flash_write_non_volatile_variables(void);
int flash_dump(void);
void flash_get_program_size(void);
void flash_get_config_size(void);
void *flash_get_config_location(CONFIG_TYPE_T config_type);

#endif