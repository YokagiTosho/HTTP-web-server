#include "config.h"

#define DEFAULT_WORKERS 1
#define DEFAULT_TIMEOUT 10000
#define DEFAULT_IP 0
#define DEFAULT_PORT 8000

static config_t CONFIG = {
	.ip           = DEFAULT_IP,
	.port         = DEFAULT_PORT,
	.workers      = DEFAULT_WORKERS,
	.poll_timeout = DEFAULT_TIMEOUT,
};

char *CONFIG_KEYS[] = {
	"Addr", "Port", "Workers", "PollTimeout", 
	"BaseDir", "CgiDir", "CgiFile", 
	"DefaultHtml", "DefaultCgi", NULL,
};

int sus_get_config_addr()
{
	return CONFIG.ip;
}

short sus_get_config_port()
{
	return CONFIG.port;
}

int sus_get_config_workers()
{
	return CONFIG.workers;
}

int sus_get_config_polltimeout()
{
	return CONFIG.poll_timeout;
}

#define RETURN_CONFIG_STR(FIELD, DEFAULT) \
	do { if (CONFIG.FIELD[0]) return CONFIG.FIELD; return DEFAULT; } while (0)

const char *sus_get_config_basedir()
{
	RETURN_CONFIG_STR(base_dir, NULL);
}

const char *sus_get_config_cgidir()
{
	RETURN_CONFIG_STR(cgi_dir, "/cgi-bin/");
}

const char *sus_get_config_cgifile()
{
	RETURN_CONFIG_STR(cgi_file, ".cgi");
}

const char *sus_get_config_default_html()
{
	RETURN_CONFIG_STR(default_html, "index.html");
}

const char *sus_get_config_default_cgi()
{
	RETURN_CONFIG_STR(default_cgi, "index.cgi");
}

static config_options_t sus_get_option(const char *key)
{
	int i;
	for (i = 0; CONFIG_KEYS[i] != NULL; i++) {
		if (strcmp(key, CONFIG_KEYS[i]) == 0) {
			return i;
		}
	}
	return -1;
}

static int sus_parse_line(const char *line, size_t linelen)
{
	char *p, key[128], value[128];
	int keylen, vallen;
	config_options_t option;

	p = strchr(line, ' ');
	if (!p) {
		return SUS_ERROR;
	}

	keylen = (int)(p-line);
	vallen = (int)(linelen-keylen-1);

	strncpy(key, line, keylen);
	key[keylen] = '\0';
	strncpy(value, line+keylen+1, vallen);
	value[vallen] = '\0';

	option = sus_get_option(key);
	switch (option) {
		case config_addr:
			if (inet_pton(AF_INET, value, &CONFIG.ip) == -1) {
				sus_log_error(LEVEL_PANIC, "Failed \"inet_pton()\": %s", strerror(errno));
				return SUS_ERROR;
			}
#ifdef DEBUG
			fprintf(stdout, "Addr: %d.%d.%d.%d\n",
					(CONFIG.ip & 0xff),
					(CONFIG.ip >> 8) & 0xff,
					(CONFIG.ip >> 16) & 0xff,
					(CONFIG.ip >> 24));
#endif
			break;
		case config_port:
			CONVERT_TO_INT(CONFIG.port, value);
#ifdef DEBUG
			fprintf(stdout, "Port: %d\n", CONFIG.port);
#endif
			break;
		case config_workers:
			CONVERT_TO_INT(CONFIG.workers, value);
			if (CONFIG.workers <= 0) {
				CONFIG.workers = DEFAULT_WORKERS;
			}
#ifdef DEBUG
			fprintf(stdout, "Workers: %d\n", CONFIG.workers);
#endif
			break;
		case config_poll_timeout:
			CONVERT_TO_INT(CONFIG.poll_timeout, value);
			if (CONFIG.poll_timeout < 0) {
				// TODO
			}
			CONFIG.poll_timeout *= 1000;
#ifdef DEBUG
			fprintf(stdout, "PollTimeout: %d\n", CONFIG.poll_timeout);
#endif
			break;
		case config_base_dir:
			strncpy(CONFIG.base_dir, value, vallen);
#ifdef DEBUG
			fprintf(stdout, "BaseDir: %s\n", CONFIG.base_dir);
#endif
			break;
		case config_cgi_dir:
			strncpy(CONFIG.cgi_dir, value, vallen);
#ifdef DEBUG
			fprintf(stdout, "CgiDir: %s\n", CONFIG.cgi_dir);
#endif
			break;
		case config_cgifile:
			strncpy(CONFIG.cgi_file, value, vallen);
#ifdef DEBUG
			fprintf(stdout, "CgiFile: %s\n", CONFIG.cgi_file);
#endif
			break;
		case config_default_html:
			strncpy(CONFIG.default_html, value, vallen);
#ifdef DEBUG
			fprintf(stdout, "DefaultHtml: %s\n", CONFIG.default_html);
#endif
			break;
		case config_default_cgi:
			strncpy(CONFIG.default_cgi, value, vallen);
#ifdef DEBUG
			fprintf(stdout, "DefaultCgi: %s\n", CONFIG.default_cgi);
#endif
			break;
		default:
			return SUS_ERROR;
	}
	return SUS_OK;
}

int sus_parse_config()
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

		if (sus_parse_line(line, linelen) == SUS_ERROR) {
#ifdef DEBUG
			fprintf(stdout, "Error on line %d\n", linen);
#endif
			sus_log_error(LEVEL_PANIC, "Error on line %d: %s", linen, line);
			return SUS_ERROR;
		}
		linen++;
	}

	if (!CONFIG.base_dir[0]) {
		sus_log_error(LEVEL_PANIC, "BaseDir was not specified in config");
		return SUS_ERROR;
	}

	fclose(config_file);
	return SUS_OK;
}
