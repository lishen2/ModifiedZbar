#include <stdio.h>
#include <stdlib.h>
#include <zbar.h>
#include <image.h>

zbar_image_scanner_t *scanner = NULL;

void mirror_buf(unsigned char* buf, int width, int height)
{
	int i, j;
	int size = width * height;
	unsigned char tmp;

	for (i = 0; i < size; i += width){
		for (j = 0; j < width/2; ++j){
			tmp = buf[i + j];
			buf[i + j] = buf[i + width - j - 1];
			buf[i + width - j - 1] = tmp;
		}
	}

	return;
}

void get_data(char* filename, int *width, int *height, void** data)
{
    FILE *fp;
	size_t readsize;
	static unsigned char buf[1024 * 768];

    fp = fopen(filename, "rb");
    if (NULL == fp){
		return;
    }

	readsize = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);

	if (sizeof(buf) != readsize){
		return;
	}

	*(unsigned char**)data = buf;
	*width = 1024;
	*height = 768;

	return;
}

void dumpraw(unsigned char *data, int size)
{
	FILE *fp;

	fp = fopen("convdump.bin", "wb");
	if (NULL == fp){
		return;
	}

	fwrite(data, 1, size, fp);
	fclose(fp);

	return;
}	

int main (int argc, char **argv)
{
    if(argc < 2) return(1);

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
    zbar_image_set_format(image, fourcc('Y', '8', '0', '0'));
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, raw, width * height, NULL);

    /*
	image = zbar_image_convert(image, fourcc('Y', '8', '0', '0'));
	if (NULL == image){
		printf("Image convert error\r\n");
	}
	*/

//	mirror_buf(image->data, 320, 240);
//	dumpraw(image->data, image->datalen);

    /* scan the image for barcodes */
    zbar_scan_image(scanner, image);

    /* extract results */
    const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
    for(; symbol; symbol = zbar_symbol_next(symbol)) {
        /* do something useful with results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        printf("decoded %s symbol \"%s\"\n",
               zbar_get_symbol_name(typ), data);
    }

    /* clean up */
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

    return(0);
}
