#include <stdio.h>
#include <stdlib.h>
#include "uv.h"
#include "http_client/http_response.h"
#include "http_client/http_request.h"
#include "http_parser/http_parser.h"
#include "http_handler/http_handler.h"

#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

void on_body_(HTTP_RESPONSE_OBJ* http_resp, const char* body, size_t length)
{
    // if(http_resp->status_code==200 || http_resp->status_code==206)
    {

    }
}

void on_complete_(HTTP_RESPONSE_OBJ* http_resp, int errcode)
{
    LOG_DEBUG("errcode:%d", errcode);
    LOG_DEBUG("code:%d, status:%s", http_resp->parser.status_code, http_resp->status);
    int i;
    for(i=0;i<http_resp->header_num;++i)
    {
        LOG_DEBUG("%s:%s", http_resp->headers[i].field, http_resp->headers[i].value);
    }
    http_response_range_filesize(http_resp);
}

static void test_request()
{
    HTTP_REQUEST_OBJ* req = (HTTP_REQUEST_OBJ*)malloc(sizeof(HTTP_REQUEST_OBJ));
    memset(req, 0, sizeof(HTTP_REQUEST_OBJ));
    char url[] = "http://www.xunlei.com";
    strncpy(req->url, url, strlen(url));
    req->method = HTTP_GET;
    add_http_range_request_head(req, 0, 1023);
    // req->data = NULL;
    req->on_complete = on_complete_;
    req->on_body = on_body_;
    http_client_request(req);

}

static uv_tcp_t server;

static int on_url(http_parser* parser, const char* at, size_t length)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
    memcpy(http_resp->request->url, at, length);
    return 0;
}

static int on_header_field(http_parser* parser, const char *at, size_t length)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
    memcpy(http_resp->request->headers[http_resp->request->header_num].field, at, length);
    return 0;
}

static int on_header_value(http_parser* parser, const char *at, size_t length)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
    memcpy(http_resp->request->headers[http_resp->request->header_num].value, at, length);
    ++http_resp->request->header_num;
    return 0;
}

static int on_headers_complete(http_parser* parser)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
    handle(http_resp);
    return 0;
}

static int on_body(http_parser* parser, const char *at, size_t length)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;
    if(http_resp->request->on_body)
        http_resp->request->on_body(http_resp, at, length);
    return 0;
}

static void on_close(uv_handle_t* handle)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)handle->data;
    free(http_resp->request);
    free(http_resp);
    LOG_DEBUG("0X%x", http_resp);
}

static void on_finish(HTTP_RESPONSE_OBJ* http_resp)
{
    uv_close((uv_handle_t*)&http_resp->request->socked, on_close);
}

static int on_message_complete(http_parser* parser)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)parser->data;

    // int i;
    // for(i=0;i<http_resp->request->header_num;++i)
    // {
    //     LOG_DEBUG("%s:%s", http_resp->request->headers[i].field, http_resp->request->headers[i].value);
    // }
    if(http_resp->request->on_complete)
        http_resp->request->on_complete(http_resp, 0);
    // http_resp->on_finish(http_resp);
    return 0;
}

static http_parser_settings parser_settings = {NULL, on_url, NULL, on_header_field, on_header_value, on_headers_complete, on_body, on_message_complete};

static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)handle->data;
    buf->base = (char*)malloc(suggested_size);
    buf->len = suggested_size;
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)stream->data;
    http_parser_execute(&http_resp->request->parser, &parser_settings, buf->base, nread);
    free(buf->base);
}

static void on_connect(uv_stream_t* server_handle, int status) 
{
    HTTP_RESPONSE_OBJ* http_resp = (HTTP_RESPONSE_OBJ*)malloc(sizeof(HTTP_RESPONSE_OBJ));
    LOG_DEBUG("status:%d, 0X%x", status, http_resp);
    memset(http_resp, 0, sizeof(HTTP_RESPONSE_OBJ));
    http_resp->on_finish = on_finish;

    HTTP_REQUEST_OBJ* req = (HTTP_REQUEST_OBJ*)malloc(sizeof(HTTP_REQUEST_OBJ));
    memset(req, 0, sizeof(HTTP_REQUEST_OBJ));
    http_resp->request = req;

    uv_tcp_init(uv_default_loop(), &http_resp->request->socked);
    http_parser_init(&http_resp->request->parser, HTTP_REQUEST);
    http_resp->request->parser.data = http_resp;
    http_resp->request->socked.data = http_resp;

    uv_accept(server_handle, (uv_stream_t*)&http_resp->request->socked);
    uv_read_start((uv_stream_t*)&http_resp->request->socked, on_alloc, on_read);
}

static void start_server()
{
    uv_tcp_init(uv_default_loop(), &server);
    struct sockaddr_in address;
    uv_ip4_addr(SERVER_IP, SERVER_PORT, &address);
    uv_tcp_bind(&server, (struct sockaddr*)&address, 0);
    uv_listen((uv_stream_t*)&server, 128, on_connect);
}

int main() 
{
    // test_request();
    start_server();
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}
