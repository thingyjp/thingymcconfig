#pragma once

#define ARGS_NAMEPREFIX       {"nameprefix", 'n', 0, G_OPTION_ARG_STRING, &nameprefix,"name prefix", NULL}
// networking stuff
#define ARGS_INTERFACE        {"interface", 'i', 0, G_OPTION_ARG_STRING, &interface, "interface", NULL}
#define ARGS_WAITFORINTERFACE {"waitforinterface", 'w', 0, G_OPTION_ARG_NONE, &waitforinterface, "wait for interface to appear", NULL}
// apps
#define ARGS_APP              {"app", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &apps, "register an app", NULL}
// crypto options
#define ARGS_CERT             {"cert", 'c', 0, G_OPTION_ARG_STRING, &cert, "device certificate", NULL}
#define ARGS_KEY              {"key", 'k', 0, G_OPTION_ARG_STRING, &key, "private key", NULL}
#define ARGS_CONFIG           {"config", 'f', 0, G_OPTION_ARG_FILENAME, &arg_config, "config file", NULL}
#define ARGS_LOGFILE          {"logfile", 'l', 0, G_OPTION_ARG_FILENAME, &arg_logfile, "log file", NULL}
