#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_
#define MAX_URL_LEN 1024
#include "uv.h"
#include "http_parser/http_parser.h"

typedef struct _tagHttpHeader
{
	char	field[128];
	char	value[128];
}HTTP_HEADER;
struct _tagHttpRequestObj;
struct _tagHttpResponseObj;
typedef void (*http_complete_cb)(struct _tagHttpResponseObj* resp, int errcode);
typedef void (*http_on_body_cb)(struct _tagHttpResponseObj* resp, const char* body, size_t length);
typedef void (*http_on_alloc_for_body_cb)(struct _tagHttpResponseObj* resp, size_t suggested_size, uv_buf_t* buf);

typedef struct _tagHttpRequestObj
{
	char	url[MAX_URL_LEN];
	int		method;
	unsigned int 	header_num;
	HTTP_HEADER	headers[20];
	char*	body;
	void*	data;

	http_on_alloc_for_body_cb on_alloc_for_body;
	http_on_body_cb on_body;
	http_complete_cb on_complete;
}HTTP_REQUEST_OBJ;

typedef struct _tagHttpResponseObj
{
	HTTP_REQUEST_OBJ*	request;
	http_parser			parser;
	char			status[64];
	unsigned int 	header_num;
	HTTP_HEADER	headers[20];

	uv_tcp_t		socked;
	uv_connect_t	connect;
	int 			cancel;
	int 			complete;
}HTTP_RESPONSE_OBJ;

char* http_request_buf(HTTP_REQUEST_OBJ* req, unsigned int* len);

#endif