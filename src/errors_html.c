#include "errors_html.h"

static const char *bad_request = "<html> \
	<head> \
		<title>400 Bad Request</title> \
	</head> \
	<body> \
		<h1>Bad Request</h1> \
		<p>Your browser(or proxy) sent a request that this server could not understand.</p> \
		<hr>\
		<p>SUS</p> \
	</body>\
</html>";

static const char *forbidden = "<html> \
	<head> \
		<title>403 Forbidden</title> \
		</head> \
		<body> \
			<h1>Forbidden</h1> \
			<p>You don't have permission to access this page on this server.</p> \
			<hr> \
			<p>SUS</p> \
		</body> \
</html>";

static const char *internal_server_error = "<html> \
	<head> \
		<title>500 Internal Server Error</title> \
	</head> \
	<body> \
		<h1>Internal Server Error</h1> \
		<p>The server encountered an internal error \
		on misconfiguration \
		and was unable \
		to complete your request</p> \
		<hr> \
		<p>SUS</p> \
	</body> \
</html>";

static const char *method_not_allowed = "<html>\
	<head> \
		<title>405 Not Allowed</title> \
	</head> \
	<body> \
		<h1>405 Method Not Allowed</h1> \
		<hr> \
		<p>SUS</p> \
	</body> \
</html>";

static const char *not_found = "<html> \
	<head> \
		<title>404 Not Found</title> \
	</head> \
	<body> \
		<h1>Not Found</h1> \
		<p>The requested URL was not found on this server</p> \
		<hr> \
		<p>SUS</p> \
	</body> \
</html>";

static const char *payload_too_large = "<html> \
	<head> \
		<title>413 Payload Too Large</title> \
	</head> \
	<body> \
		<h1>413 Request Entity Too Large</h1> \
		<hr> \
		<p>SUS</p> \
	</body> \
</html>";

static const char *uri_too_long = "<html> \
	<head> \
		<title>414 Request-URI Too Long</title> \
	</head> \
	<body> \
		<h1>414 Request-URI Too Long</h1> \
		<hr> \
		<p>SUS</p> \
	</body> \
</html>";

static const char *version_not_allowed = "<html> \
	<head> \
		<title>505 Not Allowed</title> \
	</head> \
	<body> \
		<h1>505 HTTP Version Not Allowed</h1> \
		<hr> \
		<p>SUS</p> \
	</body> \
</html>";

const char *get_html_error(int code)
{
	switch (code) {
		case HTTP_BAD_REQUEST:
			return bad_request;
		case HTTP_FORBIDDEN:
			return forbidden;
		case HTTP_INTERNAL_SERVER_ERROR:
			return internal_server_error;
		case HTTP_METHOD_NOT_ALLOWED:
			return method_not_allowed;
		case HTTP_NOT_FOUND:
			return not_found;
		case HTTP_PAYLOAD_TOO_LARGE:
			return payload_too_large;
		case HTTP_URI_TOO_LONG:
			return uri_too_long;
		case HTTP_VERSION_NOT_SUPPORTED:
			return version_not_allowed;
	}
	return NULL;
}

