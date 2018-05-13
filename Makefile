.PHONY: clean

CFLAGS="-ggdb"
LIBMICROHTTPD=`pkg-config --cflags --libs libmicrohttpd`
GLIBJSON=`pkg-config --libs --cflags json-glib-1.0`
HOSTAPD="-Ihostap/src/utils/"

thingymcconfig: thingymcconfig.c \
	hostap/src/common/wpa_ctrl.c \
	http.o
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) $(HOSTAPD) -o $@ $^

http.o: http.c http.h
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -c -o $@ $<

clean:
	-rm thingymcconfig *.o
