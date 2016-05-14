#include <string.h>
#include "config.h"
#include "zbar.h"
#include "image.h"

#ifdef ENABLE_QRCODE
# include "qrcode.h"
#endif
#include "cutter.h"

#ifdef DEBUG_CUTTER
	#define DEBUG_LEVEL  255
#endif

#include "debug.h"

#define ZBAR_CUTTER_QRCODE_RADIO   3
#define ZBAR_CUTTER_CUT_MULTIPLIER 1.5

typedef struct zbar_cutter_box
{
	int x;
	int y;
	int width;
	int height;
}zbar_cutter_box_t;

zbar_cutter_box_t g_boxes[8];
int g_boxcount;
int g_phrase;

void zbar_cutter_reset(void)
{
	memset(g_boxes, 0, sizeof(g_boxes));
	g_boxcount = 0;
	g_phrase = 1;
	return;
}

static void _zbar_cutter_calc_box(qr_point box[4], zbar_cutter_box_t *cutterbox)
{
	int maxx, maxy;
	int minx, miny;
	int i;

	maxx = 0;
	maxy = 0;
	minx = 0x7FFFFFFF;
	miny = 0x7FFFFFFF;
	
	for (i = 0; i < 4; ++i){
		if (box[i][0] > maxx){
			maxx = box[i][0];
		}
		if (box[i][0] < minx){
			minx = box[i][0];
		}
		if (box[i][1] > maxy){
			maxy = box[i][1];
		}
		if (box[i][1] < miny){
			miny = box[i][1];
		}
	}

	cutterbox->x = minx;
	cutterbox->y = miny;
	cutterbox->width = maxx - minx;
	cutterbox->height = maxy - miny;

	return;
}

void zbar_cutter_add_box(qr_point box[4])
{
    dprintf(1, "cutter add box points"
			" x1[%d] y1[%d]"
			" x2[%d] y2[%d]"
			" x3[%d] y3[%d]"
			" x4[%d] y4[%d]\n",
			box[0][0], box[0][1],
			box[1][0], box[1][1],
			box[2][0], box[2][1],
			box[3][0], box[3][1]);

	if (g_boxcount < sizeof(g_boxes)/sizeof(g_boxes[0])){
		_zbar_cutter_calc_box(box, g_boxes + g_boxcount);
		dprintf(1, "cutter add box "
					"x[%d] y[%d] "
					"width[%d] height[%d]\n",
					g_boxes[g_boxcount].x,
					g_boxes[g_boxcount].y,
					g_boxes[g_boxcount].width,
					g_boxes[g_boxcount].height);
		g_boxcount++;
	} else {
		dprintf(1, "cutter add box, too many boxes\n");
	}

	return;
}

int zbar_cutter_need_cut(zbar_image_t *img)
{
	int ret;
	int i;

	ret = 0;
	for (i = 0; i < g_boxcount; ++i){
		if (0 == g_boxes[i].width || 0 == g_boxes[i].height){
			continue;
		}

		if ((img->width / g_boxes[i].width) > ZBAR_CUTTER_QRCODE_RADIO ||
		    (img->height / g_boxes[i].height) > ZBAR_CUTTER_QRCODE_RADIO){
			ret = 1;
			break;
		}
	}

	return ret;
}

static void _zbar_cutter_calc_pos(zbar_image_t *snap, 
								  zbar_image_t *full,
								  zbar_cutter_box_t *qrbox,
								  zbar_cutter_box_t *cutbox)
{
	zbar_cutter_box_t fullbox;
	int radio;

	//convert snap box to full image box
	radio = full->width / snap->width;
	fullbox.x = qrbox->x * radio;
	fullbox.y = qrbox->y * radio;
	fullbox.width = qrbox->width * radio;
	fullbox.height = qrbox->height * radio;

	qrbox = &fullbox;
	cutbox->x = qrbox->x - (qrbox->width * (ZBAR_CUTTER_CUT_MULTIPLIER - 1))/2;
	if (cutbox->x < 0){
		cutbox->x = 0;
	}

	cutbox->width = qrbox->width * ZBAR_CUTTER_CUT_MULTIPLIER;
	if (cutbox->x + cutbox->width > full->width){
		cutbox->width = full->width - cutbox->x;
	}

	cutbox->y = qrbox->y - (qrbox->height * (ZBAR_CUTTER_CUT_MULTIPLIER - 1))/2;
	if (cutbox->y < 0){
		cutbox->y = 0;
	}

	cutbox->height = qrbox->height * ZBAR_CUTTER_CUT_MULTIPLIER;
	if (cutbox->y + cutbox->height > full->height){
		cutbox->height = full->height - cutbox->y;
	}

	return;
}

static void _zbar_cutter_do_cut(zbar_image_t *dst, zbar_image_t *src,
								int startx, int starty)
{
	int i;
	int dstoff, srcoff;

	for (i = 0; i < dst->height; ++i){
		dstoff = i * dst->width;
		srcoff = (i + starty) * src->width + startx;
		memcpy((unsigned char*)dst->data + dstoff, (unsigned char*)src->data + srcoff, dst->width);
	}

	return;
}

static void _zbar_free_data(zbar_image_t *img)
{
	free((void*)img->data);
	return;
}

zbar_image_t* zbar_cutter_do_cut(zbar_image_t *snap, zbar_image_t *full)
{
	zbar_image_t *dst;
	zbar_cutter_box_t box;
	int i;

	g_phrase = 2;
	dst = NULL;
	for (i = 0; i < g_boxcount; ++i){
		if (0 == g_boxes[i].width || 0 == g_boxes[i].height){
			continue;
		}

		dst = zbar_image_create();

		_zbar_cutter_calc_pos(snap, full, g_boxes + i, &box);
		dst->format = snap->format;
		dst->cleanup = _zbar_free_data;
		dst->width = box.width;
		dst->height = box.height;
		dst->data = malloc(dst->width * dst->height);

		_zbar_cutter_do_cut(dst, full, box.x, box.y);

		dprintf(1, "cutter do cut startx[%d] starty[%d] width[%d] heiht[%d]\n", 
				box.x, box.y, box.width, box.height);
		memset(g_boxes + i, 0, sizeof(g_boxes[0]));

		break;
	}

	return dst;
}

int zbar_cutter_get_phrase(void)
{
	return g_phrase;
}

void zbar_cutter_set_phrase(int phrase)
{
	g_phrase = phrase;
	return;
}

