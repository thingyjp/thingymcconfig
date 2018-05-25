PKGCONFIG ?= pkg-config
CFLAGS=-ggdb
LIBMICROHTTPD=`$(PKGCONFIG) --cflags --libs libmicrohttpd`
GLIBJSON=`$(PKGCONFIG) --libs --cflags json-glib-1.0`
HOSTAPD=-Ihostap/src/common/ -Ihostap/src/utils/

.PHONY: clean

thingymcconfig: thingymcconfig.c \
	http.o \
	os_unix.o \
	wpa_ctrl.o \
	network.o \
	config.o \
	utils.o
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -o $@ $^

http.o: http.c http.h
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -c -o $@ $<

network.o: network.c network.h
	$(CC) $(CFLAGS) $(GLIBJSON) $(HOSTAPD) -c -o $@ $<

config.o: config.c config.h
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

os_unix.o: hostap/src/utils/os_unix.c
	$(CC) $(CFLAGS) -c -o $@ $<

wpa_ctrl.o: hostap/src/common/wpa_ctrl.c
	$(CC) $(CFLAGS) $(HOSTAPD) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX -c -o $@ $<

clean:
	-rm thingymcconfig *.o
