.PHONY: clean

CFLAGS=-ggdb
LIBMICROHTTPD=`pkg-config --cflags --libs libmicrohttpd`
GLIBJSON=`pkg-config --libs --cflags json-glib-1.0`
HOSTAPD=-Ihostap/src/common/ -Ihostap/src/utils/
LIBNLGENL=`pkg-config --cflags --libs libnl-genl-3.0`

thingymcconfig: thingymcconfig.c \
	http.o \
	os_unix.o \
	wpa_ctrl.o \
	network.o \
	config.o \
	utils.o
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) $(LIBNLGENL) -o $@ $^

http.o: http.c http.h
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -c -o $@ $<

network.o: network.c network.h
	$(CC) $(CFLAGS) $(GLIBJSON) $(HOSTAPD) $(LIBNLGENL) -c -o $@ $<

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
