#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "zbar.h"
#include "qrcode.h"
#include "cutter.h"

#define READ_TIMEOUT 5
#define PID_FILENAME "pid"

struct parameters
{
	const char* pidfile;
	pid_t pid;
	int width;
	int height;
};

struct runtime_var
{
	zbar_image_scanner_t *scanner;
	unsigned char* imgbuf;
	int running;
};

static struct parameters g_params;
static struct runtime_var g_rtvar;

static void _zbar_init(void)
{
	/* create a reader */
	g_rtvar.scanner = zbar_image_scanner_create();

	/* configure the reader */
	zbar_image_scanner_set_config(g_rtvar.scanner, 0, ZBAR_CFG_ENABLE, 1);

	return;
}

static void _zbar_deinit(void)
{
    zbar_image_scanner_destroy(g_rtvar.scanner);
}

static int _parser_arguments(int argc, char* argv[])
{
	int i;

	g_params.width = 0;
	g_params.height = 0;
	g_params.pidfile = NULL;

	for (i = 0; i < argc; ++i){
		if (0 == strcmp(argv[i], "--width")){
			g_params.width = atoi(argv[i+1]);
		} else if (0 == strcmp(argv[i], "--height")){
			g_params.height = atoi(argv[i+1]);
		} else if (0 == strcmp(argv[i], "--pidfile")){
			g_params.pidfile = argv[i+1];
		}
	}

	if (0 == g_params.width ||
		0 == g_params.height ||
		NULL == g_params.pidfile){
		return -1;
	}

	return 0;
}

static int _read_pidfile(void)
{
	FILE *fp;
	int ret;
	int pid;
	char buf[31 + 1];
	size_t nread;

	pid = 0;
	while(0 == pid){
		ret = access(g_params.pidfile, F_OK);
		if (0 != ret){
			continue;
		}

		fp = fopen(g_params.pidfile, "rb");
		if (NULL == fp){
			continue;
		}
	
		do {
			nread = fread(buf, 1, sizeof(buf) - 1, fp);
			if (nread <= 0){
				break;;
			}

			buf[nread] = 0;
			pid = atoi(buf);
		} while(0);

		fclose(fp);
		fp = NULL;

		usleep(100 * 1000);
	}

	return pid;
}

static void _signalHandler(int sig)
{
	if (0 != g_params.pid){
		kill(g_params.pid, SIGINT);
	}

	return;
}

static unsigned char* _read_buffer(int size, int timeout)
{
	size_t nread, ntotal;
	time_t snamp;

	snamp = time(NULL);
	clearerr(stdin);
	ntotal = 0;

	while((time(NULL) - snamp) < (unsigned)timeout){

		nread = fread(g_rtvar.imgbuf + ntotal, 1, size - ntotal, stdin);
		if (nread > 0){
			ntotal += nread;
		}
		if (ntotal == size){
			break;
		}
	}

	printf("Read size [%d] feof[%d] ferror[%d] errno[%d], addr[%p]\n", ntotal, feof(stdin), ferror(stdin), errno, g_rtvar.imgbuf + ntotal);
	
	if (ntotal == size){
		return g_rtvar.imgbuf;
	} else {
		return NULL;
	}
}

static void _print_symbol(zbar_image_t *image)
{
    /* extract results */
    const zbar_symbol_t *symbol = zbar_image_first_symbol(image);
    for(; symbol; symbol = zbar_symbol_next(symbol)) {
        /* do something useful with results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        printf("decoded %s symbol \"%s\"\n",
               zbar_get_symbol_name(typ), data);
    }

	return;
}

static void _prcess_image(unsigned char* buf, int width, int height)
{
	zbar_image_t *image = NULL;
	zbar_image_t *snap = NULL;
	zbar_image_t *cutted = NULL;

    /* wrap image data */
    image = zbar_image_create();
    zbar_image_set_format(image, *(int*)"Y800");
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, buf, width * height, NULL);

	/* reset cutter */
	zbar_cutter_reset();
	zbar_cutter_set_phrase(1);

	/* convert image one quarter size snapshot */
	snap = zbar_image_downsize(image);
	if (NULL == snap){
		printf("Convert error\n");
		goto error;
	}

    /* scan the image for barcodes */
    zbar_scan_image(g_rtvar.scanner, snap);

	/* print symbol */
	_print_symbol(snap);

	/* cut and rescan */
	while(zbar_cutter_need_cut(snap)){

		zbar_cutter_set_phrase(2);
		cutted = zbar_cutter_do_cut(snap, image);
		//zbar_image_write_png(cutted, "cutted.png");
		
		/* scan the image for barcodes */
		zbar_scan_image(g_rtvar.scanner, cutted);

		/* print symbol */
		_print_symbol(cutted);

		zbar_image_destroy(cutted);
	}

error:
	if (NULL != snap){
		zbar_image_destroy(snap);
	}
	if (NULL != image){
		zbar_image_destroy(image);
	}

	return;
}

int main(int argc, char* argv[])
{
	int ret;
	unsigned char* buf;

	ret = _parser_arguments(argc, argv);
	if (0 != ret){
		printf("Arguments error\n");
		return -1;
	}

	printf("W:[%d] H:[%d] PIDfile:[%s]\n", g_params.width, g_params.height, g_params.pidfile);

	printf("waiting pid file\n");
	g_params.pid = _read_pidfile();
	printf("read PID:[%d]\n", g_params.pid);

	g_rtvar.imgbuf = malloc(g_params.width * g_params.height * 1.5);
	if (NULL == g_rtvar.imgbuf){
		printf("Can't alloc image buffer\n");
		return -1;
	}

	//used to kill raspiyuv process at exit
	signal(SIGINT, _signalHandler);

	_zbar_init();

	sleep(2);

	g_rtvar.running = 1;
	while(g_rtvar.running){

		//send signal
		printf("send singla to PID[%d]\n", g_params.pid);
		kill(g_params.pid, SIGUSR1);
		
		//read image from stdin
		printf("Read from stdin, size[%d], timeout[%d]\n", (int)(g_params.width * g_params.height * 1.5), READ_TIMEOUT);
		buf = _read_buffer(g_params.width * g_params.height * 1.5, READ_TIMEOUT);
		if (NULL == buf){
			continue;
		}

		//zbar process
		printf("Process image\n");
		_prcess_image(buf, g_params.width, g_params.height);
	}
	
	_zbar_deinit();
	free(g_rtvar.imgbuf);

	return 0;
}


