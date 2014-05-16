#include <stdlib.h>
#include "applications.h"

#include <stdio.h>
#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

#define RESPONSE_BUF \
  "HTTP/1.1 200 OK\r\n" \
  "Content-Type: text/plain\r\n" \
  "Content-Length: 5\r\n" \
  "\r\n" \
  "play\n"

static void after_write(uv_write_t* req, int status)
{
	HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)req->data;
	free(http_resp->buf);
	free(req);
	http_resp->on_finish(http_resp);
}

static void on_complete(HTTP_RESPONSE_OBJ* http_resp, int errcode)
{
	uv_write_t* w_req = (uv_write_t*)malloc(sizeof(uv_write_t));
	w_req->data = http_resp;
	char* buf = (char*)malloc(1024);
	memcpy(buf, RESPONSE_BUF, strlen(RESPONSE_BUF));
	http_resp->buf = buf;

	uv_buf_t uv_buf = uv_buf_init(buf, strlen(RESPONSE_BUF));
	uv_write(w_req, (uv_stream_t*)&http_resp->request->socked, &uv_buf, 1, after_write);

}

void handle_play(HTTP_RESPONSE_OBJ* http_resp)
{
	http_resp->request->on_complete = on_complete;
}