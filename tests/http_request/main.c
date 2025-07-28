/* testing correct HTTP request parsing */

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "request.h"
#include "http.h"

int main() {
	request_t request = {};
	char *raw_request =
		"GET / HTTP/1.1\r\n"
		"Connection:Keep-Alive\r\n"
		"Accept: text/html\r\n"
		"Accept-Language: en-US,en;q=0.5\r\n"
		"Accept-Encoding: gzip, deflate\r\n";

	int res = sus_parse_request(raw_request, &request);
	assert(res == SUS_OK);
	assert(request.method == HTTP_GET);
	assert(strcmp(request.uri, "/") == 0);
	assert(strcmp(request.accept, "text/html") == 0);
	assert(strcmp(request.accept_language, "en-US,en;q=0.5") == 0);
	assert(strcmp(request.accept_encoding, "gzip, deflate") == 0);

	puts("All tests passed");
}
