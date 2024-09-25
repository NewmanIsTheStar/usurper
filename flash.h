/**
 * Copyright (c) 2024 NewmanIsTheStar
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef FLASH_H
#define FLASH_H

int flash_read_non_volatile_variables(void);
int flash_write_non_volatile_variables(void);
int flash_dump(void);

#endif