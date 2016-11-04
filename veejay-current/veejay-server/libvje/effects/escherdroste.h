/*
 * Linux VeeJay
 *
 * Copyright(C)2002 Niels Elburg <nwelburg@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License , or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307 , USA.
 */

#ifndef ESCHERDROSTE_H
#define ESCHERDROSTE_H

#define TRN_DROSTE 0
#define TRN_LOGPOLAR 1

typedef struct {
	uint8_t r, g, b;
} Pixel;

vj_effect *escherdroste_init();
int escherdroste_malloc(int w, int h);
void escherdroste_free();
void escherdroste_apply( VJFrame *frame, int transform );
void escherdroste_free();
#endif