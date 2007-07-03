/* 
 * Linux VeeJay
 *
 * Copyright(C)2002 Niels Elburg <elburg@hio.hen.nl>
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
#include <stdint.h>
#include <libvjmem/vjmem.h>
#include "diff.h"
#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ffmpeg/avutil.h>
#include <libyuv/yuvconv.h>
#include <libvjmsg/vj-msg.h>
#include "softblur.h"
static uint8_t *static_bg = NULL;
static int take_bg_ = 0;

typedef struct
{
	uint8_t *data;
	uint8_t *current;
} diff_data;

vj_effect *diff_init(int width, int height)
{
    //int i,j;
    vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
    ve->num_params = 4;
    ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
    ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
    ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
    ve->limits[0][0] = 0;
    ve->limits[1][0] = 255;
    ve->limits[0][1] = 0;	/* reverse */
    ve->limits[1][1] = 1;
    ve->limits[0][2] = 0;	/* show mask */
    ve->limits[1][2] = 1;
    ve->limits[0][3] = 0;       /* switch to take bg mask */
    ve->limits[1][3] = 1;
    ve->defaults[0] = 20;
    ve->defaults[1] = 0;
    ve->defaults[2] = 1;
    ve->defaults[3] = 0;
    ve->description = "Map B to A (substract background mask)";
    ve->extra_frame = 1;
    ve->sub_format = 1;
    ve->has_user = 1;
    ve->user_data = NULL;
    return ve;
}

void	diff_destroy(void)
{
	if(static_bg)
		free(static_bg);
	static_bg = NULL;
	
}

#define ru8(num)(((num)+8)&~8)

int diff_malloc(void **d, int width, int height)
{
	diff_data *my;
	*d = (void*) vj_calloc(sizeof(diff_data));
	my = (diff_data*) *d;
	my->data = (uint8_t*) vj_calloc( ru8(sizeof(uint8_t) * 2 * width * height) );
	my->current = my->data + (width*height);

	if(static_bg == NULL)	
		static_bg = (uint8_t*) vj_calloc( ru8( width + width * height * sizeof(uint8_t)) );
	return 1;
}

void diff_free(void *d)
{
	if(d)
	{
		diff_data *my = (diff_data*) d;
		if(my->data) free(my->data);
		free(d);
	}
	d = NULL;
}

void diff_prepare(void *user, uint8_t *map[3], int width, int height)
{
	if(!static_bg )
	{
		veejay_msg(0,"FX \"Map B to A (substract background mask)\" not initialized");
		return;
	}
	
	veejay_memcpy( static_bg, map[0], (width*height));
	
	VJFrame tmp;
	veejay_memset( &tmp, 0, sizeof(VJFrame));
	tmp.data[0] = static_bg;
	tmp.width = width;
	tmp.height = height;
	softblur_apply( &tmp, width,height,0);

	veejay_msg(0, "Snapped and softblurred current frame to use as background mask");
}

static	void	binarify( uint8_t *dst, uint8_t *bg, uint8_t *src,int threshold,int reverse, const int len )
{
	int i;

	if(!reverse)
	{
		for( i = 0; i < len; i ++ )
		{
			if ( abs(bg[i] - src[i]) <= threshold )
				dst[i] = 0;
			else
				dst[i] = 0xff;
		}

	}
	else
	{
		for( i = 0; i < len; i ++ )
		{
			if ( abs(bg[i] - src[i]) >= threshold )
				dst[i] = 0;
			else
				dst[i] = 0xff;
		
		}
	}
}


void diff_apply(void *ed, VJFrame *frame,
		VJFrame *frame2, int width, int height, 
		int threshold, int reverse,int mode, int take_bg)
{
    
	unsigned int i;
	const uint32_t len = frame->len;
 	uint8_t *Y = frame->data[0];
	uint8_t *Cb = frame->data[1];
	uint8_t *Cr = frame->data[2];
	uint8_t *Y2 = frame2->data[0];
	uint8_t *Cb2 = frame2->data[1];
	uint8_t *Cr2 = frame2->data[2];
	diff_data *ud = (diff_data*) ed;

	if( take_bg != take_bg_ )
	{
		veejay_memcpy( static_bg, frame->data[0], frame->len );
		VJFrame tmp;
		veejay_memset( &tmp, 0, sizeof(VJFrame));
		tmp.data[0] = static_bg;
		tmp.width = width;
		tmp.height = height;
		softblur_apply( &tmp, width,height,0);
		take_bg_ = take_bg;
		return;
	}

	VJFrame *tmp = yuv_yuv_template( ud->current, NULL,NULL, width,height, 
					PIX_FMT_YUV444P );
	veejay_memcpy( ud->current, frame->data[0], len );
	softblur_apply(tmp,width,height,0);
	free(tmp);

	binarify( ud->data, static_bg, ud->current, threshold, reverse,len );

	if(mode)
	{
		veejay_memcpy( Y, ud->data, len );
		veejay_memset( Cb, 128, len );
		veejay_memset( Cr, 128, len );
		return;
	}

	uint8_t *bin = ud->data;
	for( i = 0; i < len ;i ++ )
	{
		if(bin[i])
		{
			Y[i] = Y2[i];
			Cb[i] = Cb2[i];
			Cr[i] = Cr2[i];
		}
		else
		{
			Y[i] = 0;
			Cb[i] = 128;
			Cr[i] = 128;
		}
	}
}




