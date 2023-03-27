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
	config_max_conn,
	config_workers,
} config_options_t;

typedef enum config_err {
	config_ok,
	config_not_opt,
	config_undefined_param,
} config_err_t;

typedef struct {
	int ip;
	short port;
	int max_con;
	int workers;
} config_t;


typedef struct {
	char key[128];
	char value[128];
	config_options_t opt;
} option_t;

void parse_config();

int get_config_addr();
short get_config_port();
int get_config_max_con();
int get_config_workers();

#endif
