//  evjs - Evdev Joystick Utilities
//  Copyright (C) 2020 Scott Shumate <scott@shumatech.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <stdlib.h>
#include <stdbool.h>

typedef struct barray barray_t;
typedef unsigned bit_t;

barray_t *barray_init(size_t num_bits);

void barray_free(barray_t *barray);

unsigned long *barray_data(barray_t *barray);

size_t barray_count_set(barray_t *barray);

void barray_set(barray_t *barray, bit_t bit);

void barray_clear(barray_t *barray, bit_t bit);

bool barray_is_set(barray_t *barray, bit_t bit);

typedef void (*barray_callback_t)(bit_t bit, void *arg);

void barray_foreach_set(barray_t *barray, barray_callback_t callback, void *arg);
