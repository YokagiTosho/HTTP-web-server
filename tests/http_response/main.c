/* testing correct creation of HTTP response */

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "response.h"
#include "http.h"

struct raw_response {
	char *bytes;
	int size;

	int stline_length;
	int headers_length;
	int content_length;
};

int sus_response_to_raw(struct raw_response *raw, const response_t *response);

int main() {
	const char *expected = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 15\r\n"
		"Connection: keep-alive\r\n"
		"Content-Type: text/html\r\n"
		"Server: SUS/0.2\r\n\r\n"
		"<h1>Tag H1</h1>";

	response_t response;
	struct raw_response raw;
	int ret;

	
	response.http_version = HTTP_VERSION1_1;
	response.status_code = HTTP_OK;
	response.verbose = "OK";
	
	response.connection = "keep-alive";
	response.server = "SUS/0.2";
	response.content_type = TEXT_HTML;

	response.content_length = 15;
	response.body = "<h1>Tag H1</h1>";

	ret = sus_response_to_raw(&raw, &response);
	
	assert(ret == 0);
	assert(strcmp(raw.bytes, expected) == 0);

	puts("All tests passed");
}
