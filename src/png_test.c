#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <zbar.h>

#include "qrcode.h"
#include "cutter.h"

zbar_image_scanner_t *scanner = NULL;

/* to complete a runnable example, this abbreviated implementation of
 * get_data() will use libpng to read an image file. refer to libpng
 * documentation for details
 */
static void get_data (const char *name,
                      int *width, int *height,
                      void **raw)
{
    FILE *file = fopen(name, "rb");
    if(!file) exit(2);
    png_structp png =
        png_create_read_struct(PNG_LIBPNG_VER_STRING,
                               NULL, NULL, NULL);
    if(!png) exit(3);
    if(setjmp(png_jmpbuf(png))) exit(4);
    png_infop info = png_create_info_struct(png);
    if(!info) exit(5);
    png_init_io(png, file);
    png_read_info(png, info);
    /* configure for 8bpp grayscale input */
    int color = png_get_color_type(png, info);
    int bits = png_get_bit_depth(png, info);
    if(color & PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if(color == PNG_COLOR_TYPE_GRAY && bits < 8)
        png_set_gray_1_2_4_to_8(png);
    if(bits == 16)
        png_set_strip_16(png);
    if(color & PNG_COLOR_MASK_ALPHA)
        png_set_strip_alpha(png);
    if(color & PNG_COLOR_MASK_COLOR)
        png_set_rgb_to_gray_fixed(png, 1, -1, -1);
    /* allocate image */
    *width = png_get_image_width(png, info);
    *height = png_get_image_height(png, info);
    *raw = malloc(*width * *height);
    png_bytep rows[*height];
    int i;
    for(i = 0; i < *height; i++)
        rows[i] = *raw + (*width * i);
    png_read_image(png, rows);
}

void dumpraw(unsigned char *data, int size)
{
	FILE *fp;

	fp = fopen("pngdump.bin", "wb");
	if (NULL == fp){
		return;
	}

	fwrite(data, 1, size, fp);
	fclose(fp);

	return;
}	

int main (int argc, char **argv)
{
	zbar_image_t *snap;

    if(argc < 2) return(1);

    //zbar_set_verbosity(65535);

    /* create a reader */
    scanner = zbar_image_scanner_create();

    /* configure the reader */
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    /* obtain image data */
    int width = 0, height = 0;
    void *raw = NULL;
    get_data(argv[1], &width, &height, &raw);

    /* wrap image data */
    zbar_image_t *image = zbar_image_create();
    zbar_image_set_format(image, *(int*)"Y800");
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, raw, width * height, zbar_image_free_data);

	zbar_cutter_reset();
	zbar_cutter_set_phrase(1);

	/* convert image one quarter size snapshot */
	snap = zbar_image_downsize(image);
	if (NULL == snap){
		printf("Convert error\n");
		return 0;
	}

    /* scan the image for barcodes */
    zbar_scan_image(scanner, snap);

    /* extract results */
    const zbar_symbol_t *symbol = zbar_image_first_symbol(snap);
    for(; symbol; symbol = zbar_symbol_next(symbol)) {
        /* do something useful with results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        printf("decoded %s symbol \"%s\"\n",
               zbar_get_symbol_name(typ), data);
    }

	zbar_image_t *cutimg = NULL;
	while(zbar_cutter_need_cut(snap)){

		zbar_cutter_set_phrase(2);
		cutimg = zbar_cutter_do_cut(snap, image);
		//zbar_image_write_png(cutimg, "cutted.png");

		/* scan the image for barcodes */
		int n = zbar_scan_image(scanner, cutimg);

		/* extract results */
		const zbar_symbol_t *symbol = zbar_image_first_symbol(cutimg);
		for(; symbol; symbol = zbar_symbol_next(symbol)) {
			/* do something useful with results */
			zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
			const char *data = zbar_symbol_get_data(symbol);
			printf("decoded %s symbol \"%s\"\n",
				   zbar_get_symbol_name(typ), data);
		}

		zbar_image_destroy(cutimg);
	}

    zbar_image_destroy(image);
    zbar_image_destroy(snap);
    zbar_image_scanner_destroy(scanner);

    return(0);
}
