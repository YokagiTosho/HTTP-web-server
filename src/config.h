#ifndef CONFIG_H
#define CONFIG_H

#include "base.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>

typedef enum config_options {
	config_addr,
	config_port,
	config_workers,
	config_poll_timeout,
	config_base_dir,
	config_cgi_dir,
	config_cgifile,
} config_options_t;

typedef struct {
	int ip;
	short port;
	int workers;
	int poll_timeout;
	char base_dir[255];
	char cgi_dir[255];
	char cgi_file[255];
} config_t;


int parse_config();

int get_config_addr();
short get_config_port();
int get_config_workers();

const char *get_config_basedir();
const char *get_config_cgidir();
const char *get_config_cgifile();

#endif
