#include <stdlib.h>
#include <stdio.h>
#include "http_parser/http_parser.h"
#include "http_request.h"

#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

#define get_loop uv_default_loop

static char* http_request_buf(HTTP_REQUEST_OBJ* req, unsigned int* len)
{
	struct http_parser_url url_obj = {0};
	http_parser_parse_url(req->url, strlen(req->url), 0, &url_obj);

	char* buf = (char*)malloc(1024);
	memset(buf, 0, 1024);
	snprintf(buf, 1024, "%s ", http_method_str(req->method));

	if(url_obj.field_set & (1 << UF_PATH))
	{
		strncat(buf, req->url+url_obj.field_data[UF_PATH].off, url_obj.field_data[UF_PATH].len);
	}
	else
	{
		strncat(buf, "/", 1);
	}
	strncat(buf, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"));
	int head_filed_host = 0;
	int head_filed_ua = 0;
	int i;
	for(i=0; i<req->header_num; ++i)
	{
		if(head_filed_host==0 && memcmp(req->headers[i].field, "Host", strlen("Host"))==0)
			head_filed_host = 1;
		if(head_filed_ua==0 && memcmp(req->headers[i].field, "User-Agent", strlen("User-Agent"))==0)
			head_filed_ua = 1;
		snprintf(buf+strlen(buf), 1024-strlen(buf), "%s:%s\r\n", req->headers[i].field, req->headers[i].value);
	}
	if(!head_filed_host)
	{
		snprintf(buf+strlen(buf), 1024-strlen(buf), "Host:");
		strncat(buf, req->url + url_obj.field_data[UF_HOST].off,url_obj.field_data[UF_HOST].len);
		if(!url_obj.port)
			url_obj.port = 80;
		snprintf(buf+strlen(buf), 1024-strlen(buf), ":%u\r\n",url_obj.port);
	}
	if(!head_filed_ua)
	{
		snprintf(buf+strlen(buf), 1024-strlen(buf), "User-Agent:Mozilla/5.0 (Linux; U; Android 2.3.7; en-us; Nexus One Build/FRF91) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1\r\n");
	}
	snprintf(buf+strlen(buf),1024-strlen(buf), "Connection:keep-alive\r\n");
	snprintf(buf+strlen(buf),1024-strlen(buf), "Accept:application/octet-stream\r\n");
	// snprintf(buf+strlen(buf),1024-strlen(buf), "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n");
	snprintf(buf+strlen(buf),1024-strlen(buf), "Accept-Encoding:gzip,deflate,sdch\r\n");
	snprintf(buf+strlen(buf),1024-strlen(buf), "Accept-Language:zh-CN,zh;q=0.8,en-US;q=0.6,en;q=0.4\r\n");

	strncat(buf, "\r\n", strlen("\r\n"));
	*len = strlen(buf)+1;
	return buf;
}

void add_http_range_request_head(HTTP_REQUEST_OBJ* req, uint64_t pos, uint64_t length)
{
	    // 'Range: bytes=0-499'
    char t[64] = {0};
    if(length)
	    snprintf(t, 64, "bytes=%llu-%llu", pos, length);
	else
	    snprintf(t, 64, "bytes=%llu-", pos);

	ADD_HEADER(req, "Range", t);

    // strncpy(req->headers[req->header_num].field, "Range", strlen("Range"));
    // strncpy(req->headers[req->header_num].value, t, strlen(t));
    // ++req->header_num;
}

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

static void delete_response(HTTP_RESPONSE_OBJ* http_resp)
{
	free(http_resp);
}

static void on_close(uv_handle_t* handle)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)handle->data;
	http_resp->request->on_complete(http_resp, http_resp->errcode);
	delete_response(http_resp);
}

static int on_headers_complete(http_parser* parser)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	if(parser->status_code==301)
	{
		return 1;
	}
	return 0;
}

static int on_body(http_parser* parser, const char *at, size_t length)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	http_resp->request->on_body(http_resp, at, length);
	return 0;
}

static int on_message_complete(http_parser* parser)
{
	LOG_DEBUG("");
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
	uv_close((uv_handle_t*)&http_resp->request->socked, on_close);
	// http_resp->request->on_complete(http_resp, 0);
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
	if(http_resp->cancel)
	{
		uv_close((uv_handle_t*)&http_resp->request->socked, on_close);
		http_resp->errcode = 1;
		// http_resp->request->on_complete(http_resp, 1);
	}
}

static void after_write(uv_write_t* req, int status)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)req->data;
	free(http_resp->buf);
	if(status<0 || http_resp->cancel)
	{
		if(http_resp->cancel)
			status = 1;
		else
			LOG_DEBUG("error %d, %s: %s", status, uv_err_name(status), uv_strerror(status));
		uv_close((uv_handle_t*)&http_resp->request->socked, on_close);
		http_resp->errcode = status;
		// http_resp->request->on_complete(http_resp, status);
		free(req);
		return;
	}
	uv_read_start((uv_stream_t*)&http_resp->request->socked, on_alloc, on_read);
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
		uv_close((uv_handle_t*)&http_resp->request->socked, on_close);
		http_resp->errcode = status;
		// http_resp->request->on_complete(http_resp, status);
		return;
	}
	uv_write_t* w_req = (uv_write_t*)malloc(sizeof(uv_write_t));
	w_req->data = req->data;
	unsigned int len = 0;
	char* buf = http_request_buf(http_resp->request, &len);
	http_resp->buf = buf;
	LOG_DEBUG("%s", buf);

	http_parser_init(&http_resp->parser, HTTP_RESPONSE);

	uv_buf_t uv_buf = uv_buf_init(buf, len);
	uv_write(w_req, (uv_stream_t*)&http_resp->request->socked, &uv_buf, 1, after_write);
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
	uv_tcp_init(get_loop(), &http_resp->request->socked);
	http_resp->request->socked.data = http_resp;
	http_resp->connect.data = http_resp;
	uv_tcp_connect(&http_resp->connect, &http_resp->request->socked, (struct sockaddr*)res->ai_addr, on_connect);

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
		
		// struct addrinfo hints;
	 //    hints.ai_family = PF_INET;
	 //    hints.ai_socktype = SOCK_STREAM;
	 //    hints.ai_protocol = IPPROTO_TCP;
	 //    hints.ai_flags = 0;

	    uv_getaddrinfo_t* resolver = (uv_getaddrinfo_t*)malloc(sizeof(uv_getaddrinfo_t));
	    resolver->data = resp;
   		if(!u.port)
			u.port = 80;
		char p[10] = {0};
		snprintf(p, 10, "%u", u.port);
	    // uv_getaddrinfo(get_loop(), resolver, on_resolved, tmp, p, &hints);
	    uv_getaddrinfo(get_loop(), resolver, on_resolved, tmp, p, NULL);
	    return resp;
	}
	return NULL;
}

void http_client_cancel(HTTP_RESPONSE_OBJ* resp)
{
	resp->cancel = 1;
}
