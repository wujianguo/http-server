//
//  http_connection.h
//  YouSirCmd
//
//  Created by 吴建国 on 15/12/31.
//  Copyright © 2015年 wujianguo. All rights reserved.
//

#ifndef http_connection_h
#define http_connection_h

#include <stdio.h>
#define YOU_LOG_DEBUG(format, ...)  printf("[debug:%s:%d] " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define YOU_LOG_INFO(format, ...)   printf("[info:%s:%d] " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define YOU_LOG_WARN(format, ...)   printf("[warn:%s:%d] " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define YOU_LOG_ERROR(format, ...)  printf("[error:%s:%d] " format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)


#include <assert.h>
/* ASSERT() is for debug checks, CHECK() for run-time sanity checks.
 * DEBUG_CHECKS is for expensive debug checks that we only want to
 * enable in debug builds but still want type-checked by the compiler
 * in release builds.
 */
#if defined(NDEBUG)
# define ASSERT(exp)
# define CHECK(exp)   do { if (!(exp)) abort(); } while (0)
# define DEBUG_CHECKS (0)
#else
# define ASSERT(exp)  assert(exp)
# define CHECK(exp)   assert(exp)
# define DEBUG_CHECKS (1)
#endif

#define UNREACHABLE() CHECK(!"Unreachable code reached.")

/* This macro looks complicated but it's not: it calculates the address
 * of the embedding struct through the address of the embedded struct.
 * In other words, if struct A embeds struct B, then we can obtain
 * the address of A by taking the address of B and subtracting the
 * field offset of B in A.
 */
#define CONTAINER_OF(ptr, type, field)                                        \
((type *) ((char *) (ptr) - ((char *) &((type *) 0)->field)))


#include "libuv/include/uv.h"
#include "http-parser/http_parser.h"
#include "queue.h"


#define MAX_HOST_LEN 128
#define MAX_URL_LEN 1024
#define MAX_HTTP_FIELD_LEN 56
#define MAX_HTTP_VALUE_LEN 1024
#define MAX_REQUEST_HEADER_LEN 2048

typedef struct http_connection http_connection;

struct http_header_field_value {
    char field[MAX_HTTP_FIELD_LEN];
    char value[MAX_HTTP_VALUE_LEN];
    
    QUEUE node;
};

struct http_header {
    http_parser parser;
    struct http_parser_url url;
    char url_buf[MAX_URL_LEN];
    
    QUEUE headers;
};

struct http_connection_settings {
    void (*on_connect)(http_connection *conn, void *user_data);
    void (*on_send)(http_connection *conn, void *user_data);
    void (*on_header_complete)(http_connection *conn, struct http_header *header, void *user_data);
    void (*on_body)(http_connection *conn, const char *at, size_t length, void *user_data);
    void (*on_message_complete)(http_connection *conn, void *user_data);
    void (*on_chunk_header)(http_connection *conn, void *user_data);
    void (*on_chunk_complete)(http_connection *conn, void *user_data);
};

http_connection* create_http_connection(uv_loop_t *loop, struct http_connection_settings settings, void *user_data);

http_connection* create_passive_http_connection(uv_stream_t *server, struct http_connection_settings settings, void *user_data);

int http_connection_connect(http_connection *conn, const char host[MAX_HOST_LEN], unsigned short port);

int http_connection_send(http_connection *conn, const char *buf, size_t len);

void free_http_connection(http_connection *conn);

void http_connection_stop_read(http_connection *conn);

void http_connecton_start_read(http_connection *conn);


#endif /* http_connection_h */
