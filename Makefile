.PHONY: clean

LIBMICROHTTPD=`pkg-config --cflags --libs libmicrohttpd`
GLIBJSON=`pkg-config --libs --cflags json-glib-1.0`
HOSTAPD="-Ihostap/src/utils/"

thingymcconfig: thingymcconfig.c hostap/src/common/wpa_ctrl.c
	$(CC) $(LIBMICROHTTPD) $(GLIBJSON) $(HOSTAPD) -o $@ $^

clean:
	-rm thingymcconfig
