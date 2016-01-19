//
//  http_server.h
//  YouSirCmd
//
//  Created by 吴建国 on 15/12/29.
//  Copyright © 2015年 wujianguo. All rights reserved.
//

#ifndef http_server_h
#define http_server_h

#include "queue.h"
#include "http_connection.h"

typedef struct http_request http_request;

typedef void (*http_session_complete_cb)(http_request *req);

typedef struct http_request {
    http_connection *conn;
    struct http_header *header;
    uv_loop_t *loop;
    
    http_session_complete_cb complete;
    void *user_data; // user data
} http_request;


typedef void (*handler_callback)(http_request *req);
typedef void (*handler_callback_data)(http_request *req, const char *at, size_t length);

typedef struct {
    handler_callback on_header_complete;
    handler_callback_data on_body;
    handler_callback on_message_complete;
    handler_callback on_send;
    handler_callback on_chunk_header;
    handler_callback on_chunk_complete;
    void (*on_error)(http_request *req, int err_code);
    char path[128];
    QUEUE node;
} http_handler_setting;

typedef struct {
    const char *bind_host;
    unsigned short bind_port;
    unsigned int idle_timeout;
    
    QUEUE handlers;
} http_server_config;

int http_server_run(const http_server_config *cf, uv_loop_t *loop);

#endif /* http_server_h */
