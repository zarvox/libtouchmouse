# TODO: make this work in a cross-platform manner, using HIDAPI's different backends

CC=gcc
CFLAGS=-I./hidapi/hidapi `pkg-config --cflags libusb-1.0`
LIBS=`pkg-config --libs libusb-1.0 libudev` -lpthread

touchmouse: touchmouse.o hid-libusb.o
	${CC} -o touchmouse touchmouse.o hid-libusb.o ${LIBS}

touchmouse.o: touchmouse.c

#hid.o: hidapi/linux/hid.c
#	${CC} -c ${CFLAGS} -o hid.o hidapi/linux/hid.c

hid-libusb.o: hidapi/linux/hid-libusb.c
	${CC} -c ${CFLAGS} -o hid-libusb.o hidapi/linux/hid-libusb.c

clean:
	rm -f touchmouse *.o
