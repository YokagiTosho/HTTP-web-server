#include "config.h"

static config_t CONFIG = {
	.ip      = 0,
	.port    = 8000,
	.workers = 1,
	.poll_timeout = 10000,
};

char *CONFIG_KEYS[] = {
	"Addr", "Port", "Workers", "PollTimeout", 
	"BaseDir", "CgiDir", "CgiFile", NULL,
};

int get_config_addr()
{
	return CONFIG.ip;
}

short get_config_port()
{
	return CONFIG.port;
}

int get_config_workers()
{
	return CONFIG.workers;
}

int get_config_polltimeout()
{
	return CONFIG.poll_timeout;
}

const char *get_config_basedir()
{
	if (strlen(CONFIG.base_dir)) {
		return CONFIG.base_dir;
	}
	return NULL;
}
const char *get_config_cgidir()
{
	if (strlen(CONFIG.cgi_dir)) {
		return CONFIG.cgi_dir;
	}
	return NULL;

}
const char *get_config_cgifile()
{
	if (strlen(CONFIG.cgi_file)) {
		return CONFIG.cgi_file;
	}
	return NULL;
}

static int get_option(const char *key)
{
	int i;
	for (i = 0; CONFIG_KEYS[i] != NULL; i++) {
		if (strcmp(key, CONFIG_KEYS[i]) == 0) {
			return i;
		}
	}
	return -1;
}

static int parse_line(const char *line, size_t linelen)
{
	char *p, key[128], value[128];
	int keylen, vallen, option;

	p = strchr(line, ' ');
	if (!p) {
		return SUS_ERROR;
	}

	keylen = (int)(p-line);
	vallen = (int)(linelen-keylen-1);

	strncpy(key, line, keylen);
	key[keylen] = 0;
	strncpy(value, line+keylen+1, vallen);
	value[vallen] = 0;

	option = get_option(key);
	switch (option) {
		case config_addr:
			inet_pton(AF_INET, value, &CONFIG.ip);
			break;
		case config_port:
			CONFIG.port = atoi(value);
			break;
		case config_workers:
			CONFIG.workers = atoi(value);
			break;
		case config_poll_timeout:
			/* TODO */
			//CONFIG.poll_timeout = 
			break;
		case config_base_dir:
			strncpy(CONFIG.base_dir, value, vallen);
			break;
		case config_cgi_dir:
			strncpy(CONFIG.cgi_dir, value, vallen);
			break;
		case config_cgifile:
			strncpy(CONFIG.cgi_file, value, vallen);
			break;
		default:
			return SUS_ERROR;
	}
#ifdef DEBUG
	fprintf(stdout, "%s: %s\n", key, value);
#endif
	return SUS_OK;
}

int parse_config()
{
	char line[256];
	int linen = 0, linelen;

	FILE *config_file = fopen("config", "r");
	if (!config_file) {
		sus_log_error(LEVEL_PANIC, "Could not open config: %s", strerror(errno));
		return SUS_ERROR;
	}

	while (fgets(line, 256, config_file)) {
		if (line[0] == '#' || line[0] == '\n') {
			linen++;
			continue;
		}
		linelen = strlen(line);
		line[--linelen] = 0;

		if (parse_line(line, linelen) == SUS_ERROR) {
#ifdef DEBUG
			fprintf(stdout, "Error on line %d\n", linen);
#endif
			sus_log_error(LEVEL_PANIC, "Error on line %d: %s", linen, line);
			return SUS_ERROR;
		}
		linen++;
	}
	return SUS_OK;
}

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
}
#endif
