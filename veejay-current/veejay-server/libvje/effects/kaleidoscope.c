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

#include "common.h"
#include <libvjmem/vjmem.h>
#include "kaleidoscope.h"

vj_effect *kaleidoscope_init(int w, int h)
{

	vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
	ve->num_params = 1;
	ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
	ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
	ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
	ve->defaults[0] = 0;
	ve->limits[0][0] = 0;	/* horizontal or vertical mirror */
	ve->limits[1][0] = 0;
	ve->sub_format = 1;
	ve->description = "Kaleidoscope !";
	ve->extra_frame = 0;
	ve->has_user = 0;
	ve->param_description = vje_build_param_list(ve->num_params, "H or V mode");
	return ve;
}

static void kaleidoscope(uint8_t * yuv[3], int width, int height)
{
	unsigned int x, y;

	unsigned int yi, yi2;
	const unsigned int hlen = height / 2;
	uint8_t p, cb, cr;
	const float sin60 = sin(M_PI/3);
	const float cos60 = cos(M_PI/3);

	for (y = hlen; y < height; y++) {
		unsigned int vlen = y/2;

		yi = y * width;
		yi2 = (y * sin60 + x * cos60) * width;
		for (x = 0; x < vlen ; x++) {
			
			unsigned int x2 = -x * sin60 + y * cos60;
			

			p = yuv[0][yi2 + x2];
			yuv[0][yi + x ] = p;
			yuv[0] = 255;

			cb = yuv[1][yi2 + x2];
			cr = yuv[2][yi2 + x2];
			yuv[1][yi + x] = cb;
			yuv[2][yi + x] = cr;
			
		}
	}
}

void kaleidoscope_apply( VJFrame *frame, int type)
{
	switch (type) {
		case 0:
			kaleidoscope(frame->data, frame->width, frame->height);
		break;
	}
}
