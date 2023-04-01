#ifndef CONFIG_H
#define CONFIG_H

#include "base.h"

#include <arpa/inet.h>
#include <limits.h>

typedef enum config_option {
	config_addr,
	config_port,
	config_workers,
	config_poll_timeout,
	config_base_dir,
	config_cgi_dir,
	config_cgifile,
	config_default_html,
	config_default_cgi,
} config_options_t;

typedef struct {
	int ip;
	int workers;
	int poll_timeout;

	short port;

	char base_dir[255];
	char cgi_dir[255];
	char cgi_file[255];
	char default_html[255];
	char default_cgi[255];
} config_t;


int parse_config();

int get_config_addr();
short get_config_port();
int get_config_workers();
int get_config_polltimeout();

const char *get_config_basedir();
const char *get_config_cgidir();
const char *get_config_cgifile();
const char *get_config_default_html();
const char *get_config_default_cgi();

#endif
