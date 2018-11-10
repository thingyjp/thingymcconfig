project('thingymcconfig', 'c')

if(meson.is_cross_build())
  wpasupplicant = '/usr/sbin/wpa_supplicant'
else
  wpasupplicant = find_program(['/sbin/wpa_supplicant', '/usr/sbin/wpa_supplicant'], required: true).path()
endif

src = ['thingymcconfig.c',
       'http.c',
       'network.c',
       'network_wpasupplicant.c',
       'network_dhcp.c',
       'network_dns.c',
       'network_model.c',
       'config.c',
       'utils.c',
       'certs.c',
       'logging.c',
       'ctrl.c',
       'apps.c',
       'tbus.c',
       'hostap/src/common/wpa_ctrl.c',
       'hostap/src/utils/os_unix.c']
       
deps = [ dependency('glib-2.0'),
         dependency('json-glib-1.0'),
         dependency('gio-2.0'),
         dependency('gio-unix-2.0'),
         dependency('libmicrohttpd'),
         dependency('libgpiod'),
         dependency('libteenynet'),
         dependency('libnlglue') ]

hostapincdir = ['hostap/src/common/','hostap/src/utils/']
jsonmacrosincdir = ['json-glib-macros']

add_global_arguments('-Werror=implicit-function-declaration', language : 'c')
add_global_arguments('-DWPASUPPLICANT_BINARYPATH="' + wpasupplicant + '"', language : 'c')
add_global_arguments('-DCONFIG_CTRL_IFACE', language : 'c')
add_global_arguments('-DCONFIG_CTRL_IFACE_UNIX', language : 'c')

executable('thingymcconfig', src, include_directories : include_directories(hostapincdir + jsonmacrosincdir),
           dependencies : deps, install : true, install_dir : 'sbin')

conf_data = configuration_data()
conf_data.set('debugbuild', get_option('debugbuild') ? 1 : 0)
configure_file(input : 'buildconfig.h.in',
               output : 'buildconfig.h',
               configuration : conf_data)

install_headers(['include/thingymcconfig/ctrl.h',
                 'include/thingymcconfig/utils.h',
                 'include/thingymcconfig/logging.h'],
                 subdir : 'thingymcconfig')

client_glib_deps = [ dependency('glib-2.0'),
                     dependency('gio-unix-2.0') ]
client_glib_src = [ 'client_glib.c', 'tbus.c', 'utils.c', 'logging.c' ]
client_glib_lib = shared_library('thingymcconfig_client_glib', client_glib_src,
                dependencies : client_glib_deps,
                install : true)
install_headers('include/thingymcconfig/client_glib.h', subdir : 'thingymcconfig')

pkg = import('pkgconfig')
pkg.generate(client_glib_lib,
			 version : '0.1',
             filebase : 'libthingymcconfig_client_glib')
