
CC=gcc
LD=gcc

CINCS=-Iinc -I.
CFLAGS=-g -Wall -Wno-parentheses

LDINCS=
LDFLAGS=-lpng -lrt -Wall

SRCS=convert.o cutter.o symbol.c decoder.c error.c decoder/ean.c decoder/i25.c\
      decoder/code39.c decoder/qr_finder.c decoder/code128.c\
      refcnt.c image.c qrcode/qrdectxt.c qrcode/rs.c qrcode/util.c qrcode/binarize.c\
      qrcode/isaac.c qrcode/bch15_5.c qrcode/qrdec.c img_scanner.c scanner.c
OBJS=$(patsubst %c, %o, $(SRCS))

all : png yuv stdin tags

png : $(OBJS) png_test.o
	$(LD) -o png_test $^ $(LDINCS) $(LDFLAGS)

yuv : $(OBJS) yuv_test.o
	$(LD) -o yuv_test $^ $(LDINCS) $(LDFLAGS)

stdin : $(OBJS) stdin_test.o
	$(LD) -o stdin_test $^ $(LDINCS) $(LDFLAGS)

.PHONY : tags
tags : 
	ctags -R *.c
	cscope -Rqb *.c

%.o : %.c
	$(CC) -c $(CFLAGS) $(CINCS) -o $@ $<

clean:
	-rm -f png_test yuv_test stdin_test
	-rm -f $(OBJS)
	-rm -f *.o
	-rm -f cscope.* tags


