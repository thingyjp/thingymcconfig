PKGCONFIG ?= pkg-config
CFLAGS=-ggdb -Werror=implicit-function-declaration
LIBMICROHTTPD=`$(PKGCONFIG) --cflags --libs libmicrohttpd`
GLIB=`$(PKGCONFIG) --libs --cflags glib-2.0 gio-2.0 gio-unix-2.0`
GLIBJSON=`$(PKGCONFIG) --libs --cflags json-glib-1.0`
HOSTAPD=-Ihostap/src/common/ -Ihostap/src/utils/

LIBNLGENL=`$(PKGCONFIG) --cflags --libs libnl-genl-3.0`
LIBNLROUTE=`$(PKGCONFIG) --cflags --libs libnl-route-3.0`

COMMONHEADERS=buildconfig.h

ifdef WPASUPPLICANT_BINARYPATH
	CFLAGS+= -D WPASUPPLICANT_BINARYPATH=\"$(WPASUPPLICANT_BINARYPATH)\"
endif

.PHONY: clean install

thingymcconfig: thingymcconfig.o \
	http.o \
	os_unix.o \
	wpa_ctrl.o \
	network.o \
	network_nl80211.o \
	network_rtnetlink.o \
	network_wpasupplicant.o \
	network_dhcp.o \
	network_model.o \
	config.o \
	utils.o \
	certs.o \
	dhcp4_model.o \
	dhcp4_client.o \
	dhcp4_server.o \
	packetsocket.o \
	logging.o \
	ctrl.o
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) $(LIBNLGENL) $(LIBNLROUTE) -o $@ $^

thingymcconfig.o: thingymcconfig.c ctrl.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -c -o $@ $<

http.o: http.c http.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(LIBMICROHTTPD) $(GLIBJSON) -c -o $@ $<

network.o: network.c network.h network_priv.h network_wpasupplicant.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) $(HOSTAPD) $(LIBNLGENL) $(LIBNLROUTE) -c -o $@ $<

network_nl80211.o: network_nl80211.c network_nl80211.h network_priv.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) $(LIBNLGENL) $(LIBNLROUTE) -c -o $@ $<

network_rtnetlink.o: network_rtnetlink.c network_rtnetlink.h network_priv.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIB) $(LIBNLROUTE) -c -o $@ $<

network_wpasupplicant.o: network_wpasupplicant.c network_wpasupplicant.h network_wpasupplicant_priv.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) $(HOSTAPD) -c -o $@ $<

network_dhcp.o: network_dhcp.c network_dhcp.h \
	dhcp4_client.h dhcp4_server.h ip4.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

network_model.o: network_model.c network_model.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

config.o: config.c config.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

utils.o: utils.c utils.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIBJSON) -c -o $@ $<

certs.o: certs.c certs.h $(COMMONHEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

dhcp4_model.o: dhcp4_model.c dhcp4_model.h dhcp4.h
	$(CC) $(CFLAGS) $(GLIB) -c -o $@ $<

dhcp4_client.o: dhcp4_client.c dhcp4_client.h \
	dhcp4_model.h dhcp4.h packetsocket.h ip4.h
	$(CC) $(CFLAGS) $(GLIB) -c -o $@ $<

dhcp4_server.o: dhcp4_server.c dhcp4_server.h \
	dhcp4.h dhcp4_model.h packetsocket.h
	$(CC) $(CFLAGS) $(GLIB) -c -o $@ $<

packetsocket.o: packetsocket.c packetsocket.h \
	$(COMMONHEADERS)
	$(CC) $(CFLAGS) $(GLIB) -c -o $@ $<

logging.o: logging.c logging.h
	$(CC) $(CFLAGS) $(GLIB) -c -o $@ $<

ctrl.o: ctrl.c ctrl.h
	$(CC) $(CFLAGS) $(GLIB) $(GLIBJSON) -c -o $@ $<

#wpa_supplicant provided bits

os_unix.o: hostap/src/utils/os_unix.c
	$(CC) $(CFLAGS) -c -o $@ $<

wpa_ctrl.o: hostap/src/common/wpa_ctrl.c
	$(CC) $(CFLAGS) $(HOSTAPD) -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX -c -o $@ $<

install:

clean:
	-rm thingymcconfig *.o
