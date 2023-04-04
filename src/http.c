#include "http.h"

const char *sus_http_method_to_str(int http_method)
{
	switch (http_method) {
		case HTTP_GET:     return "GET";
		case HTTP_POST:    return "POST";
		case HTTP_HEAD:    return "HEAD";
		case HTTP_OPTIONS: return "OPTIONS";
		case HTTP_PUT:     return "PUT";
		case HTTP_DELETE:  return "DELETE";
		//case HTTP_PATCH:   return "PATCH";
		default:           return "UNDEFINED HTTP METHOD";
	}
	return "NULL";
}

const char *sus_http_version_to_str(int http_version)
{
	switch (http_version) {
		case HTTP_VERSION1_1:
			return "HTTP/1.1";
	}
	return "UNDEFINED HTTP VERSION";
}



int sus_get_content_type(const char *filepath)
{
	char *p;
	int typ;
	
	p = strchr(filepath, '.');
	if (!p) {
		return TEXT_PLAIN;
	}

	p++;
#if 0
	if (!strcmp(p, "json")) {
		return APPLICATION_JSON;
	}
	else if (!strcmp(p, "js")) {
		return APPLICATION_JAVASCRIPT;
	}
	else if (!strcmp(p, "html")) {
		return TEXT_HTML;
	}
	else if (!strcmp(p, "css")) {
		return TEXT_CSS;
	}
	else if (!strcmp(p, "csv")) {
		return TEXT_CSV;
	}
	else if (!strcmp(p, "xml")) {
		return TEXT_XML;
	}
	else if (!strcmp(p, "jpeg") || !strcmp(p, "jpg")) {
		return IMAGE_JPEG;
	}
	else if (!strcmp(p, "png")) {
		return IMAGE_PNG;
	}
#endif
	CONTENT_TYPE_FROM_STR(typ, p);
	return typ;
}

const char *sus_content_type_to_str(int content_type)
{
	switch (content_type) {
		case TEXT_PLAIN:             return "text/plain";
		case TEXT_CSS:               return "text/css";
		case TEXT_CSV:               return "text/csv";
		case TEXT_HTML:              return "text/html";
		case TEXT_XML:               return "text/xml";

		case IMAGE_JPEG:             return "image/jpeg";
		case IMAGE_PNG:              return "image/png";
		case IMAGE_SVG_XML:          return "image/svg+xml";

		case APPLICATION_JAVASCRIPT: return "application/javascript";
		case APPLICATION_JSON:       return "application/json";
		case APPLICATION_XML:        return "application/xml";
	}
	return NULL;
}

const char *sus_set_verbose(int status_code)
{
#define SET_VERBOSE(STR) return STR
	switch (status_code) {
		case HTTP_CONTINUE: 
			SET_VERBOSE("Continue");
			break;
		case HTTP_SWITCHING_PROTOCOLS: 
			SET_VERBOSE("Switching protocols");
			break;

		case HTTP_OK:
			SET_VERBOSE("OK");
			break;
		case HTTP_CREATED:
			SET_VERBOSE("Created");
			break;
		case HTTP_ACCEPTED:
			SET_VERBOSE("Accepted");
			break;
		case HTTP_NON_AUTHORITATIVE_INFORMATION:
			SET_VERBOSE("Non-Authoritative Information");
			break;
		case HTTP_NO_CONTENT:
			SET_VERBOSE("No Content");
			break;
		case HTTP_RESET_CONTENT:
			SET_VERBOSE("Reset Content");
			break;
		case HTTP_PARTIAL_CONTENT:
			SET_VERBOSE("Partial Content");
			break;

		case HTTP_MULTIPLE_CHOICES:
			SET_VERBOSE("Multiple Choices");
			break;
		case HTTP_MOVED_PERMANENTLY:
			SET_VERBOSE("Moved Permanently");
			break;
		case HTTP_FOUND:
			SET_VERBOSE("Found");
			break;
		case HTTP_SEE_OTHER:
			SET_VERBOSE("See Other");
			break;
		case HTTP_NOT_MODIFIED:
			SET_VERBOSE("Not Modified");
			break;
		case HTTP_TEMPORARY_REDIRECT:
			SET_VERBOSE("Temporary Redirect");
			break;
		case HTTP_PERMANENT_REDIRECT:
			break;
			SET_VERBOSE("Permanent Redirect");

		case HTTP_BAD_REQUEST:
			SET_VERBOSE("Bad Request");
			break;
		case HTTP_UNAUTHORIZED:
			SET_VERBOSE("Unauthorized");
			break;
		case HTTP_PAYMENT_REQUIRED:
			SET_VERBOSE("Payment Required");
			break;
		case HTTP_FORBIDDEN:
			SET_VERBOSE("Forbidden");
			break;
		case HTTP_NOT_FOUND:
			SET_VERBOSE("Not Found");
			break;
		case HTTP_METHOD_NOT_ALLOWED:
			SET_VERBOSE("Method Not Allowed");
			break;
		case HTTP_NOT_ACCEPTABLE:
			SET_VERBOSE("Not Acceptable");
			break;
		case HTTP_PROXY_AUTHENTICATION_REQUIRED:
			SET_VERBOSE("Proxy Authentication Required");
			break;
		case HTTP_REQUEST_TIMEOUT:
			SET_VERBOSE("Request Timeout");
			break;
		case HTTP_CONFLICT:
			SET_VERBOSE("Conflict");
			break;
		case HTTP_GONE:
			SET_VERBOSE("Gone");
			break;
		case HTTP_LENGTH_REQUIRED:
			SET_VERBOSE("Length Required");
			break;
		case HTTP_PRECONDITION_FAILED:
			SET_VERBOSE("Precondition Failed");
			break;
		case HTTP_PAYLOAD_TOO_LARGE:
			SET_VERBOSE("Payload Too Large");
			break;
		case HTTP_URI_TOO_LONG:
			SET_VERBOSE("URI Too Long");
			break;
		case HTTP_UNSUPPORTED_MEDIA_TYPE:
			SET_VERBOSE("Unsupported Media Type");
			break;
		case HTTP_RANGE_NOT_SATISFIABLE:
			SET_VERBOSE("Range Not Satisfiable");
			break;
		case HTTP_EXPECTATION_FAILED:
			SET_VERBOSE("Expectation Failed");
			break;
		case HTTP_TOO_EARLY:
			SET_VERBOSE("Too Early");
			break;
		case HTTP_UPGRADE_REQUIRED:
			SET_VERBOSE("Upgrade Required");
			break;
		case HTTP_PRECONDITION_REQUIRED:
			SET_VERBOSE("Precondition Required");
			break;
		case HTTP_TOO_MANY_REQUESTS:
			SET_VERBOSE("Too Many Requests");
			break;
		case HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE:
			SET_VERBOSE("Request Header Fields Too Large");
			break;
		case HTTP_UNAVAILABLE_FOR_LEGAL_REASONS:
			SET_VERBOSE("Unavailable For Legal Reasons");
			break;

		case HTTP_INTERNAL_SERVER_ERROR:
			SET_VERBOSE("Internal Server Error");
			break;
		case HTTP_NOT_IMPLEMENTED:
			SET_VERBOSE("Not Implemented");
			break;
		case HTTP_BAD_GATEWAY:
			SET_VERBOSE("Bad Gateway");
			break;
		case HTTP_SERVICE_UNAVAILABLE:
			SET_VERBOSE("Service Unavailable");
			break;
		case HTTP_GATEWAY_TIMEOUT:
			SET_VERBOSE("Gateway Timeout");
			break;
		case HTTP_VERSION_NOT_SUPPORTED:
			SET_VERBOSE("HTTP Version Not Supported");
			break;
		case HTTP_VARIANT_ALSO_NEGOTIATES:
			SET_VERBOSE("Variant Also Negotiates");
			break;
		case HTTP_NOT_EXTENDED:
			SET_VERBOSE("Not Extended");
			break;
		case HTTP_NETWORK_AUTHENTICATION_REQUIRED:
			SET_VERBOSE("Network Authentication Required");
			break;
		default: sus_log_error(LEVEL_PANIC, "Undefined HTTP status code: %d", status_code);
	}
	return "UNDEFINED";
}
