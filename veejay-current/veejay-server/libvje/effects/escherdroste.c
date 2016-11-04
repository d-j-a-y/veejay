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

/*
Adapted from https://github.com/tcoxon/droste
*/

#include "common.h"
#include <libvjmem/vjmem.h>
#include <math.h>
#include "escherdroste.h"


Pixel transpColor;
/* Variables that are constant across a single image */
static const double two_pi = 2.0 * M_PI;
double origin_x, origin_y;
double r1, r2, r2_over_r1, log_r1;
double a, period, cosa, sina, cosacosa, cosasina;

Pixel pixel(uint8_t r, uint8_t g, uint8_t b) { //TODO YUV
	Pixel result = {r, g, b};
	return result;
}

int pixel_eq(Pixel x, Pixel y) { //TODO YUV
	return x.r == y.r && x.g == y.g && x.b == y.b;
}

vj_effect *escherdroste_init(int w, int h)
{
	vj_effect *ve = (vj_effect *) vj_calloc(sizeof(vj_effect));
	ve->num_params = 1;
	ve->defaults = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* default values */
	ve->limits[0] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* min */
	ve->limits[1] = (int *) vj_calloc(sizeof(int) * ve->num_params);	/* max */
	ve->defaults[0] = 0;	/* default value of first parameter. Here it's "droste" */
	ve->limits[0][0] = 0;	/* droste or logpolar */
	ve->limits[1][0] = 1;
    ve->sub_format = 1;
	ve->description = "Escher droste";
	ve->extra_frame = 0;
	ve->has_user = 0;
	ve->param_description = vje_build_param_list(ve->num_params, "Droste or Logpolar");
	return ve;
}

static uint8_t *buf[3] = { NULL,NULL,NULL };

int escherdroste_malloc(int w, int h)
{
	buf[0] = (uint8_t*) vj_malloc(sizeof(uint8_t) * RUP8(w * h  *  3 ) );
	if(!buf[0]) return 0;
	buf[1] = buf[0] + (w*h);
	buf[2] = buf[1] + (w*h);
}

void escherdroste_free()
{
	if(buf[0])
		free(buf[0]);
	buf[0] =  NULL;
	buf[1] =  NULL;
	buf[2] =  NULL;
}

/* Rotate the coordinates so that diagonal coincides with the imaginary
   axis and shrunk to be 2pi high.
    Where z = x+jy, and a = atan(log(r2/r1)/2pi),
    z' = z cos(a) e**ja
    or  x' = x cos(a) cos(a) - y cos(a) sin(a)
        y' = y cos(a) cos(a) + x cos(a) sin(a)
 */
void rotate(double *rx, double *ry, double x, double y) {
    *rx = x*cosacosa - y*cosasina;
    *ry = y*cosacosa + x*cosasina;
}

/* Transform x and y into logarithmic polar coordinates:
    where z = x+jy,
    z' = log(z)
    or  x' = log(sqrt(x*x + y*y))
        y' = atan(y/x)
 */
void to_logpolar(double *rx, double *ry, double x, double y) {
    *rx = log(sqrt(x*x + y*y)) - log_r1;
    *ry = atan2(y, x);
}

/* Transform log-polar coords x and y into cartesian coordinates.
    Where z = x+jy,
    z' = e**z
    or  x' = cos(y) e**x
        y' = sin(y) e**x */
void to_cartes(double *rx, double *ry, double x, double y) {
    double e_x = exp(x + log_r1);
    *rx = cos(y) * e_x;
    *ry = sin(y) * e_x;
}

void init_transform(uint8_t * yuv[3], const int width, const int height) 
{
    transpColor = pixel(yuv[0][width*height/2 + width/2],
                        yuv[1][width*height/2 + width/2],
                        yuv[2][width*height/2 + width/2]);
/////DEBUG
//	printf("Transparent (hex color): #%02x%02x%02x\n",
//        transpColor.r, transpColor.g, transpColor.b);

    /* Centre of the transformation, currently always at
       the centre of the image, but could really be anywhere */
    origin_x = width/2.0;
    origin_y = height/2.0;
//    printf("Centre at (%f, %f)\n", origin_x, origin_y);

    /* Calculate r2_over_r1 (used to calculate the period of repetition).
       Currently assumes the origin is at the centre of the image.
       r2 is the outer radius (beyond which may lie transparent pixels).
       r1 is the inner radius (within which may lie transparent pixels). */
    r2 = origin_y < origin_x ? origin_y : origin_x;

    int i;
    r1 = 0.0;
    for (i = 0; i < width * height; i++) {
        if (pixel_eq(pixel(yuv[0][i], yuv[1][i], yuv[2][i]), transpColor)) {
            double x = i % width - origin_x,
                   y = i / width - origin_y;
            double r = sqrt(x*x + y*y);
            if (r > r1)
                r1 = r;
        }
    }
//    printf("r2 = %f, r1 = %f\n", r2, r1);
    
    r2_over_r1 = r2/r1;
    log_r1 = log(r1);

    /* Set up some values that are constant across the image */
    period = log(r2_over_r1);
    a = atan2(period, two_pi);
    cosa = cos(a);
    sina = sin(a);
    cosacosa = cosa*cosa;
    cosasina = cosa*sina;
}

static void transform_droste(uint8_t * yuv[3], const int width, const int height) 
{
    int i;
    int repeat_min = -2, repeat_max = 10;

    init_transform(yuv, width, height);

    for (i = 0; i < width*height; i++) {
        int j, k;
        double x = i % width,
               y = i / width;

        buf[0][i] = transpColor.r;
        buf[1][i] = transpColor.g;
        buf[2][i] = transpColor.b;

        x -= origin_x;
        y -= origin_y;

        to_logpolar(&x, &y, x, y);

        for (j = repeat_min; j <= repeat_max; j++) {
            double x1 = x, y1 = y;
            x1 += period*j;
            rotate(&x1, &y1, x1, y1);
            to_cartes(&x1, &y1, x1, y1);

            x1 += origin_x;
            y1 += origin_y;

            if (0 <= x1 && x1 < width &&
                0 <= y1 && y1 < height)
            {
                k = (int)x1 + (int)y1 * width;
                if (!pixel_eq(pixel(yuv[0][k], yuv[1][k], yuv[2][k]), transpColor)) {
					buf[0][i] = yuv[0][k];
					buf[1][i] = yuv[1][k];
					buf[2][i] = yuv[2][k];
                    break;
                }
            }
        }
    }

    for (i = 0; i < width*height; i++) {
		yuv[0][i] = buf[0][i];
		yuv[1][i] = buf[1][i];
		yuv[2][i] = buf[2][i];
	}

}

static void transform_logpolar(uint8_t * yuv[3], const int width, const int height/*,
                               const int lp_rotate, const int lp_repeat*/)
{
    int i;
    int repeat_min = -5, repeat_max = 5;
/*
    if (!do_repeat)
        repeat_min = repeat_max = 0;
*/
    init_transform(yuv, width, height);

    for (i = 0; i < width*height; i++) {
        int j, k;
        double x = i % width,
               y = i / width;

        buf[0][i] = transpColor.r;
        buf[1][i] = transpColor.g;
        buf[2][i] = transpColor.b;

        x -= origin_x;
        y -= origin_y;

        /* display the image in log-polar coordinates */
        x *= log(r2) / width;
        y *= two_pi / height;

        for (j = repeat_min; j <= repeat_max; j++) {
            double x1 = x, y1 = y;
            x1 += period*j;
//            if (do_rotate)
                rotate(&x1, &y1, x1, y1);
            to_cartes(&x1, &y1, x1, y1);

            x1 += origin_x;
            y1 += origin_y;

            if (0 <= x1 && x1 < width &&
                0 <= y1 && y1 < height)
            {
                k = (int)x1 + (int)y1 * width;
                if (!pixel_eq(pixel(yuv[0][k], yuv[1][k], yuv[2][k]), transpColor)) {
					buf[0][i] = yuv[0][k];
					buf[1][i] = yuv[1][k];
					buf[2][i] = yuv[2][k];
                    break;
                }
            }
        }
    }

    for (i = 0; i < width*height; i++) {
		yuv[0][i] = buf[0][i];
		yuv[1][i] = buf[1][i];
		yuv[2][i] = buf[2][i];
	}

}

/*
{
	int x, y;
	int yi, yi2;
	const int hlen = height/2;
	const int vlen = width/2;
	uint8_t p, cb, cr;

	p = 0;

	for (y = height/4; y < hlen; y++) {
		yi = y * width;
		for (x = width/4; x < vlen ; x++) {

			yuv[0][yi + x ] = p; //luma set by parameter

//			yuv[1][yi + x] = cb; //don't touch nor blue
//			yuv[2][yi + x] = cr; // and red chrominance

		}
	}
}


{
	int x, y;
	int yi, yi2;
	const int hlen = height/2;
	const int vlen = width/2;
	uint8_t p, cb, cr;

	p = 255;

	for (y = height/4; y < hlen; y++) {
		yi = y * width;
		for (x = width/4; x < vlen ; x++) {

			yuv[0][yi + x ] = p; //luma set by parameter

//			yuv[1][yi + x] = cb; //don't touch nor blue
//			yuv[2][yi + x] = cr; // and red chrominance

		}
	}
}
*/

void escherdroste_apply( VJFrame *frame, int transform)
{
	const int width = frame->width;
	const int height = frame->height;

	switch (transform) {
	case TRN_DROSTE:
		transform_droste(frame, width, height);
		break;
	case TRN_LOGPOLAR:
		transform_logpolar(frame, width, height/*, lp_rotate, lp_repeat*/);
		break;
	}
}