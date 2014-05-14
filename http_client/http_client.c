#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uv.h"
#include "http_client.h"
#include "http_parser/http_parser.h"

#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

static int on_status(http_parser* parser, const char *at, size_t length)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	memcpy(http_resp->status, at, length);
	return 0;
}

static int on_header_field(http_parser* parser, const char *at, size_t length)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	memcpy(http_resp->headers[http_resp->header_num].field, at, length);
	return 0;
}

static int on_header_value(http_parser* parser, const char *at, size_t length)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	memcpy(http_resp->headers[http_resp->header_num].value, at, length);
	++http_resp->header_num;
	return 0;
}

static int on_headers_complete(http_parser* parser)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	return 0;
}

static int on_body(http_parser* parser, const char *at, size_t length)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	http_resp->request->on_body(http_resp, at, length);
	return 0;
}

static void delete_response(HTTP_RESPONSE_OBJ* http_resp)
{
	free(http_resp);
}

static void on_close(uv_handle_t* handle)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)handle->data;
	delete_response(http_resp);
}

static int on_message_complete(http_parser* parser)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	uv_close((uv_handle_t*)&http_resp->socked, on_close);
	http_resp->request->on_complete(http_resp, 0);
	http_resp->complete = 1;
	return 0;
}

static http_parser_settings parser_settings = {NULL, NULL, on_status, on_header_field, on_header_value, on_headers_complete, on_body, on_message_complete};

static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)handle->data;
	buf->base = (char*)malloc(suggested_size);
	buf->len = suggested_size;
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	// LOG_DEBUG("%s\n", buf->base);
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)stream->data;
	http_parser_execute(&http_resp->parser, &parser_settings, buf->base, nread);
	free(buf->base);
	if(http_resp->cancel && !http_resp->complete)
	{
		uv_close((uv_handle_t*)&http_resp->socked, on_close);
		http_resp->request->on_complete(http_resp, 1);
	}
}

static void after_write(uv_write_t* req, int status)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)req->data;
	if(status<0 || http_resp->cancel)
	{
		if(http_resp->cancel)
			status = 1;
		else
			LOG_DEBUG("error %d, %s: %s", status, uv_err_name(status), uv_strerror(status));
		uv_close((uv_handle_t*)&http_resp->socked, on_close);
		http_resp->request->on_complete(http_resp, status);
		free(req);
		return;
	}
	uv_read_start((uv_stream_t*)&http_resp->socked, on_alloc, on_read);
	free(req);
}

static void on_connect(uv_connect_t* req, int status)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)req->data;
	if(status<0 || http_resp->cancel)
	{
		if(http_resp->cancel)
			status = 1;
		else
			LOG_DEBUG("error %s: %s", uv_err_name(status), uv_strerror(status));
		uv_close((uv_handle_t*)&http_resp->socked, on_close);
		http_resp->request->on_complete(http_resp, status);
		return;
	}
	uv_write_t* w_req = (uv_write_t*)malloc(sizeof(uv_write_t));
	w_req->data = req->data;
	unsigned int len = 0;
	char* buf = http_request_buf(http_resp->request, &len);
	// LOG_DEBUG("%s", buf);

	http_parser_init(&http_resp->parser, HTTP_RESPONSE);

	uv_buf_t uv_buf = uv_buf_init(buf, len);
	uv_write(w_req, (uv_stream_t*)&http_resp->socked, &uv_buf, 1, after_write);
}

static void on_resolved(uv_getaddrinfo_t *r, int status, struct addrinfo *res)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)r->data;
	if(status<0 || http_resp->cancel)
	{
		if(http_resp->cancel)
			status = 1;
		else
			LOG_DEBUG("error %s: %s", uv_err_name(status), uv_strerror(status));
		http_resp->request->on_complete(http_resp, status);
    	uv_freeaddrinfo(res);
	    free(r);
		return;
	}
	uv_tcp_init(uv_default_loop(), &http_resp->socked);
	http_resp->socked.data = http_resp;
	http_resp->connect.data = http_resp;
	uv_tcp_connect(&http_resp->connect, &http_resp->socked, (struct sockaddr*)res->ai_addr, on_connect);

    uv_freeaddrinfo(res);
    free(r);
}

HTTP_RESPONSE_OBJ* http_client_request(HTTP_REQUEST_OBJ* req)
{
	char* url = req->url;
	char tmp[1024] = {0};
	struct http_parser_url u = {0};
	http_parser_parse_url(url, strlen(url), 0, &u);


	if(u.field_set & (1 << UF_HOST))
	{
	    HTTP_RESPONSE_OBJ* resp = (HTTP_RESPONSE_OBJ*)malloc(sizeof(HTTP_RESPONSE_OBJ));
	    memset(resp, 0, sizeof(HTTP_RESPONSE_OBJ));
	    resp->parser.data = resp;
	    resp->request = req;
		strncpy(tmp, url+u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);
		tmp[u.field_data[UF_HOST].len+1]='\0';
		
		struct addrinfo hints;
	    hints.ai_family = PF_INET;
	    hints.ai_socktype = SOCK_STREAM;
	    hints.ai_protocol = IPPROTO_TCP;
	    hints.ai_flags = 0;

	    uv_getaddrinfo_t* resolver = (uv_getaddrinfo_t*)malloc(sizeof(uv_getaddrinfo_t));
	    resolver->data = resp;
   		if(!u.port)
			u.port = 80;
		char p[10] = {0};
		snprintf(p, 10, "%u", u.port);
	    uv_getaddrinfo(uv_default_loop(), resolver, on_resolved, tmp, p, &hints);
	    return resp;
	}
	return NULL;
}

void http_client_cancel(HTTP_RESPONSE_OBJ* resp)
{
	resp->cancel = 1;
}
