#include <stdio.h>
#include <stdlib.h>
#include "uv.h"
#include "http_client/http_client.h"
#include "http_client/http_request.h"
#include "http_parser/http_parser.h"

#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

void on_body(HTTP_RESPONSE_OBJ* http_resp, const char* body, size_t length)
{
    LOG_DEBUG("");
}

void on_complete(HTTP_RESPONSE_OBJ* http_resp, int errcode)
{
    LOG_DEBUG("errcode:%d", errcode);
    LOG_DEBUG("code:%d, status:%s", http_resp->parser.status_code, http_resp->status);
    int i;
    for(i=0;i<http_resp->header_num;++i)
    {
        LOG_DEBUG("%s:%s", http_resp->headers[i].field, http_resp->headers[i].value);
    }
}

int main() 
{
    HTTP_REQUEST_OBJ* req = (HTTP_REQUEST_OBJ*)malloc(sizeof(HTTP_REQUEST_OBJ));
    memset(req, 0, sizeof(HTTP_REQUEST_OBJ));
    char url[] = "http://www.xunlei.com";
    strncpy(req->url, url, strlen(url));
    req->method = HTTP_GET;
    // req->data = NULL;
    req->on_complete = on_complete;
    req->on_body = on_body;
    http_client_request(req);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}
