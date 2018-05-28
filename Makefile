PKGCONFIG ?= pkg-config
CFLAGS=-ggdb
LIBMICROHTTPD=`$(PKGCONFIG) --cflags --libs libmicrohttpd`
GLIBJSON=`$(PKGCONFIG) --libs --cflags json-glib-1.0`
HOSTAPD=-Ihostap/src/common/ -Ihostap/src/utils/

LIBNLGENL=`pkg-config --cflags --libs libnl-genl-3.0`
LIBNLROUTE=`pkg-config --cflags --libs libnl-route-3.0`

.PHONY: clean

thingymcconfig: thingymcconfig.c \
	http.o \
	os_unix.o \
	wpa_ctrl.o \
	network.o \
	config.o \
	utils.o \
	certs.o
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) $(LIBNLGENL) -o $@ $^

http.o: http.c http.h
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -c -o $@ $<

network.o: network.c network.h
	$(CC) $(CFLAGS) $(GLIBJSON) $(HOSTAPD) $(LIBNLGENL) $(LIBNLROUTE) -c -o $@ $<

config.o: config.c config.h
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

certs.o: certs.c certs.h
	$(CC) $(CFLAGS) -c -o $@ $<

os_unix.o: hostap/src/utils/os_unix.c
	$(CC) $(CFLAGS) -c -o $@ $<

wpa_ctrl.o: hostap/src/common/wpa_ctrl.c
	$(CC) $(CFLAGS) $(HOSTAPD) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX -c -o $@ $<

clean:
	-rm thingymcconfig *.o
