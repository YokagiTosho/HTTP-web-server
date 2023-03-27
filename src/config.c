#include "config.h"

static config_t CONFIG = {
	.ip      = 0,
	.port    = 8000,
	.max_con = 50,
	.workers = 1,
};

int get_config_addr()
{
	return CONFIG.ip;
}

short get_config_port()
{
	return CONFIG.port;
}

int get_config_max_con()
{
	return CONFIG.max_con;
}

int get_config_workers()
{
	return CONFIG.workers;
}

static void get_config_opt_val(const option_t *opt) {
	//int res;
	switch (opt->opt) {
		case config_addr:
			inet_pton(AF_INET, opt->value, &CONFIG.ip);
			/* TODO error check */
#ifdef DEBUG
			printf("addr: %d\n", CONFIG.ip);
#endif
			break;
		case config_port:
			CONFIG.port = atoi(opt->value);
#ifdef DEBUG
			printf("port: %d\n", CONFIG.port);
#endif
			/* TODO error check */
			break;
		case config_max_conn:
			CONFIG.max_con = atoi(opt->value);
#ifdef DEBUG
			printf("max_con: %d\n", CONFIG.max_con);
#endif
			/* TODO error check */
			break;
		case config_workers:
			CONFIG.workers = atoi(opt->value);
			if (CONFIG.workers > MAX_WORKERS) {
				sus_log_error(LEVEL_WARNING, "workers %d > max workers %d: setting to default(1)\n", CONFIG.workers, MAX_WORKERS);
				CONFIG.workers = 1;
			}
#ifdef DEBUG
			printf("workers: %d\n", CONFIG.workers);
#endif
			/* TODO error check */
			break;
	}
}

static config_err_t parse_opt(const char *line, option_t *opt)
{
	char *space = NULL;
	const char *curr = NULL;
	int i;

	space = strchr(line, ' ');
	if (!space) {
		return config_not_opt;
	}

	for (i = 0, curr = line; curr != space; ++curr, ++i) {
		opt->key[i] = *curr;
	} opt->key[i] = '\0';

	for (i = 0, curr = space+1; *curr; ++curr, ++i) {
		opt->value[i] = *curr;
	} opt->value[i] = '\0';

	if (!strcmp(opt->key, "addr")) {
		opt->opt = config_addr;
	}
	else if (!strcmp(opt->key, "port")) {
		opt->opt = config_port;
	}
#if 0
	else if (!strcmp(opt->key, "max_con")) {
		opt->opt = config_max_conn;
	}
#endif
	else if (!strcmp(opt->key, "workers")) {
		opt->opt = config_workers;
	}
	else {
		return config_undefined_param;
	}

	return config_ok;
}

void parse_config()
{
	char line[256];
	int res;
	option_t opt;

	FILE *config_file = fopen("config", "r");
	if (!config_file) {
		sus_log_error(LEVEL_PANIC, "Could not open config: %s", strerror(errno));
		exit(1);
	}

	while (fgets(line, 256, config_file)) {
		if (line[0] == '#' || line[0] == '\n') {
			/* if its comment or empty line skip it */
			continue;
		}

		res = parse_opt(line, &opt);
		switch (res) {
			case config_ok:
				get_config_opt_val(&opt);
				break;
			case config_not_opt:
				sus_log_error(LEVEL_WARNING, "Not an option!: %s", line);
				continue;
				break;
			case config_undefined_param:
				sus_log_error(LEVEL_PANIC, "Error config_undefined_param!: %s", line);
				continue;
				break;
			default:
				sus_log_error(LEVEL_PANIC, "Failed at parse_opt");
				continue;
				break;
		}
	}
	fclose(config_file);

#if 0
#ifdef DEBUG
	fprintf(stdout, "addr: %d.%d.%d.%d, port: %d, max_con: %d\n",
			(CONFIG.ip & 0xff),
			(CONFIG.ip >> 8) & 0xff,
			(CONFIG.ip >> 16) & 0xff,
			(CONFIG.ip >> 24),
			CONFIG.port,
			CONFIG.max_con);
#endif
#endif
}
