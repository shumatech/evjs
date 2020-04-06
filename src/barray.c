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
#include "util.h"
#include "barray.h"

#define BITS_PER_LONG        (sizeof(unsigned long) * 8)
#define BITS_TO_LONGS(n)     (((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)

struct barray
{
    size_t num_bits;
    size_t num_longs;
    unsigned long data[0];
};

barray_t *barray_init(size_t num_bits)
{
    size_t num_longs = BITS_TO_LONGS(num_bits);
    barray_t *barray = xalloc(sizeof(unsigned long) * num_longs + sizeof(barray_t));
    barray->num_bits = num_bits;
    barray->num_longs = num_longs;
    return barray;
}

void barray_free(barray_t *barray)
{
    xfree(barray);
}

unsigned long *barray_data(barray_t *barray)
{
    return barray->data;
}

size_t barray_count_set(barray_t *barray)
{
    size_t count = 0;
    for (int i = 0; i < barray->num_longs; i++)
        count += __builtin_popcountl(barray->data[i]);
    return count;
}

void barray_set(barray_t *barray, bit_t bit)
{
    if (bit >= barray->num_bits)
        return;

    int index = bit / BITS_PER_LONG;
    int shift = bit % BITS_PER_LONG;

    barray->data[index] |= (1 << shift);
}

void barray_clear(barray_t *barray, bit_t bit)
{
    if (bit >= barray->num_bits)
        return;

    int index = bit / BITS_PER_LONG;
    int shift = bit % BITS_PER_LONG;

    barray->data[index] &= ~(1 << shift);
}

bool barray_is_set(barray_t *barray, bit_t bit)
{
    if (bit >= barray->num_bits)
        return false;

    int index = bit / BITS_PER_LONG;
    int shift = bit % BITS_PER_LONG;
    return (barray->data[index] & (1 << shift)) != 0;
}

void barray_foreach_set(barray_t *barray, barray_callback_t callback, void *arg)
{
    for (int i = 0; i < barray->num_longs; i++)
    {
        unsigned long bits = barray->data[i];
        while (bits != 0)
        {
            callback(i * BITS_PER_LONG + __builtin_ctzl(bits), arg);
            bits ^= (bits & -bits);
        }
    }    
}