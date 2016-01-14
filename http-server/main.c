//
//  main.c
//  http-server
//
//  Created by wujianguo on 16/1/1.
//  Copyright © 2016年 wujianguo. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "http_server.h"

#define DEFAULT_BIND_HOST     "127.0.0.1"
#define DEFAULT_BIND_PORT     9013
#define DEFAULT_IDLE_TIMEOUT  (5 * 1000)

#define HTTP_SERVER_ADD_HANDLER(queue, custom_path, custom_handler)         \
    do {                                                                    \
        http_handler_setting setting = {0};                                 \
        strncpy(setting.path, custom_path, sizeof(setting.path));           \
        setting.on_send = custom_handler##_on_send;                         \
        setting.on_body = custom_handler##_on_body;                         \
        setting.on_message_complete = custom_handler##_on_message_complete; \
        setting.on_header_complete = custom_handler##_on_header_complete;   \
        QUEUE_INIT(&setting.node);                                          \
        QUEUE_INSERT_TAIL(queue, &setting.node);                            \
    }                                                                       \
    while(0)


const static char default_http[] = "HTTP/1.1 200 OK\r\n"
"Server: YouSir/2\r\n"
"Content-type: text/plain\r\n"
"Content-Length: 5\r\n"
"\r\n"
"hello";

// /
static void root_handler_on_header_complete(http_request *req) {
    
}

static void root_handler_on_body(http_request *req, const char *at, size_t length) {
    
}

static void root_handler_on_message_complete(http_request *req) {
    http_connection_send(req->conn, default_http, strlen(default_http));
}

static void root_handler_on_send(http_request *req) {
    req->complete(req);
}



static void on_connect(http_connection *conn, void *user_data) {
    const char format[] = "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_2) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.106 Safari/537.36\r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Accept-Language: zh-CN,zh;q=0.8,en-US;q=0.6,en;q=0.4\r\n"
        "\r\n";
    http_connection_send(conn, format, strlen(format));
}

static void on_send(http_connection *conn, void *user_data) {
    YOU_LOG_DEBUG("");
}

static void on_header_complete(http_connection *conn, struct http_header *header, void *user_data) {
    YOU_LOG_DEBUG("");
}

static void on_body(http_connection *conn, const char *at, size_t length, void *user_data) {
    char *s = (char*)malloc(length+1);
    memset(s, 0, length+1);
    memcpy(s, at, length);
    YOU_LOG_DEBUG("%p:%s", conn, s);
    free(s);
}

static void on_message_complete(http_connection *conn, void *user_data) {
    free_http_connection(conn);
}

static struct http_connection_settings settings = {
    on_connect,
    on_send,
    on_header_complete,
    on_body,
    on_message_complete,
    NULL,
    NULL
};

static void on_timer_expire(uv_timer_t *handle) {
    http_connection *conn = create_http_connection(handle->loop, settings, (void*)1);
    http_connection_connect(conn, "127.0.0.1", DEFAULT_BIND_PORT);
}


#include "utility.h"
int main(int argc, char **argv) {
    
//    char url_buf[] = "http://baidu.com/snds/ene?key=value";
//    char url_buf[] = "http://baidu.com/snds/ene?s=nene&key=value";
    char url_buf[] = "http://baidu.com/snds/ene?key=ss";
//    char url_buf[] = "http://baidu.com/snds/ene?key=value&val=sns";
    struct http_parser_url url = {0};
    http_parser_url_init(&url);
    http_parser_parse_url(url_buf, strlen(url_buf), 0, &url);
    char key[] = "key";
    size_t off = 0;
    size_t len = 0;
    get_query_argument(&url, url_buf, key, strlen(key), &off, &len);
    char value[1024] = {0};
    memcpy(value, url_buf+off, len);
    printf("off:%zu, len:%zu, %s\n", off, len, value);
    return 0;
    
    
    http_server_config config;
    
    memset(&config, 0, sizeof(config));
    config.bind_host = DEFAULT_BIND_HOST;
    config.bind_port = DEFAULT_BIND_PORT;
    config.idle_timeout = DEFAULT_IDLE_TIMEOUT;
    
    QUEUE_INIT(&config.handlers);
    HTTP_SERVER_ADD_HANDLER(&config.handlers, "/", root_handler);
    
    uv_timer_t timer_handle;
    uv_timer_init(uv_default_loop(), &timer_handle);
    uv_timer_start(&timer_handle, on_timer_expire, 1000, 0);
    
    return http_server_run(&config, uv_default_loop());
}
