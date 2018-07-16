project('thingymcconfig', 'c')

src = ['thingymcconfig.c',
       'http.c',
       'network.c',
       'network_nl80211.c',
       'network_rtnetlink.c',
       'network_wpasupplicant.c',
       'network_dhcp.c',
       'network_model.c',
       'config.c',
       'utils.c',
       'certs.c',
       'dhcp4_model.c',
       'dhcp4_client.c',
       'dhcp4_server.c',
       'packetsocket.c',
       'logging.c',
       'ctrl.c',
       'apps.c',
       'hostap/src/common/wpa_ctrl.c',
       'hostap/src/utils/os_unix.c']
       
deps = [ dependency('glib-2.0'),
         dependency('json-glib-1.0'),
         dependency('gio-2.0'),
         dependency('gio-unix-2.0'),
         dependency('libmicrohttpd'),
         dependency('libnl-genl-3.0'),
         dependency('libnl-route-3.0')]

hostapincdir = include_directories(['hostap/src/common/','hostap/src/utils/'])
add_global_arguments('-DCONFIG_CTRL_IFACE', language : 'c')
add_global_arguments('-DCONFIG_CTRL_IFACE_UNIX', language : 'c')

executable('thingymcconfig', src, include_directories : hostapincdir, dependencies : deps, install : true)
