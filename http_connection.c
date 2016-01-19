//
//  http_connection.c
//  YouSirCmd
//
//  Created by 吴建国 on 15/12/31.
//  Copyright © 2015年 wujianguo. All rights reserved.
//

#include "http_connection.h"

#include <stdlib.h>

enum http_connection_state {
    s_idle,
    s_connecting,
    s_connected,

    s_message_begin,
    s_url,
    s_status,
    s_header_field,
    s_header_value,
    s_header_complete,
    s_body,
    s_message_complete,
    
    s_closing,
    s_dead
};

typedef struct http_connection {
    enum http_connection_state state;
    struct http_connection_settings settings;
    
    unsigned int idle_timeout;
//    uv_timer_t timer_handle; // todo: time out
    uv_write_t write_req;
    
    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t tcp;
        uv_udp_t udp;
    } handle;
    
    union {
        uv_getaddrinfo_t addrinfo_req;
        uv_connect_t connect_req;
        uv_req_t req;
        struct sockaddr_in6 addr6;
        struct sockaddr_in addr4;
        struct sockaddr addr;
        char buf[2048];  /* Scratch space. Used to read data into. */
    } t;
    
    uv_loop_t *loop;
    
    char *send_buf;
    char *recv_buf;

    int callbacking;
    int destroy;

    struct http_header header;
    void *user_data;
} http_connection;

static int on_message_begin(http_parser *parser) {
    http_connection *conn = (http_connection*)parser->data;
    conn->state = s_message_begin;
    YOU_LOG_DEBUG("conn: %p", conn);
    return 0;
}

static int on_url(http_parser *parser, const char *at, size_t length) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    if (conn->state != s_url) {
        memset(conn->header.url_buf, 0, sizeof(conn->header.url_buf));
    }
    strncat(conn->header.url_buf, at, length);
    conn->state = s_url;
    return 0;
}

static int on_status(http_parser *parser, const char *at, size_t length) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    conn->state = s_status;
    return 0;
}

static int on_header_field(http_parser *parser, const char *at, size_t length) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    if (conn->state != s_header_field || conn->state != s_header_value) {
        http_parser_url_init(&conn->header.url);
        http_parser_parse_url(conn->header.url_buf, strlen(conn->header.url_buf), 1, &conn->header.url);
    }
    if (conn->state != s_header_field) {
        // new
        struct http_header_field_value *header = (struct http_header_field_value*)malloc(sizeof(struct http_header_field_value));
        memset(header, 0, sizeof(struct http_header_field_value));
        QUEUE_INIT(&header->node);
        QUEUE_INSERT_HEAD(&conn->header.headers, &header->node);
    }
    QUEUE *q = QUEUE_HEAD(&conn->header.headers);
    struct http_header_field_value *head = QUEUE_DATA(q, struct http_header_field_value, node);
    strncat(head->field, at, length);
    
    conn->state = s_header_field;
    return 0;
}

static int on_header_value(http_parser *parser, const char *at, size_t length) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    QUEUE *q = QUEUE_HEAD(&conn->header.headers);
    struct http_header_field_value *head = QUEUE_DATA(q, struct http_header_field_value, node);
    strncat(head->value, at, length);
    conn->state = s_header_value;
    return 0;
}

static void log_http_header(struct http_header *header) {
    if (header->parser.type == HTTP_REQUEST) {
        YOU_LOG_DEBUG("%s request: %s", http_method_str(header->parser.method), header->url_buf);
    } else {
        YOU_LOG_DEBUG("response: %d %s", header->parser.status_code, header->url_buf);
    }
    
    struct http_header_field_value *head;
    QUEUE *q;
    QUEUE_FOREACH(q, &header->headers) {
        head = QUEUE_DATA(q, struct http_header_field_value, node);
        YOU_LOG_DEBUG("%s: %s", head->field, head->value);
    }
}

static int on_headers_complete(http_parser *parser) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p ************header************", conn);
    log_http_header(&conn->header);
    YOU_LOG_DEBUG("conn: %p **********header end************", conn);
    conn->state = s_header_complete;
    if (conn->settings.on_header_complete) {
        conn->settings.on_header_complete(conn, &conn->header, conn->user_data);
    }
    return 0;
}

static int on_body(http_parser *parser, const char *at, size_t length) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    conn->state = s_body;
    if (conn->settings.on_body) {
        conn->settings.on_body(conn, at, length, conn->user_data);
    }
    return 0;
}

static int on_message_complete(http_parser *parser) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    conn->state = s_message_complete;
    if (conn->settings.on_message_complete) {
        conn->settings.on_message_complete(conn, conn->user_data);
    }
    return 0;
}

static int on_chunk_header(http_parser *parser) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p, %llu", conn, parser->content_length);
    if (conn->settings.on_chunk_header) {
        conn->settings.on_chunk_header(conn, conn->user_data);
    }
    return 0;
}

static int on_chunk_complete(http_parser *parser) {
    http_connection *conn = (http_connection*)parser->data;
    YOU_LOG_DEBUG("conn: %p", conn);
    if (conn->settings.on_chunk_complete) {
        conn->settings.on_chunk_complete(conn, conn->user_data);
    }
    return 0;
}


static struct http_parser_settings parser_setting = {
    on_message_begin,
    on_url,
    on_status,
    on_header_field,
    on_header_value,
    on_headers_complete,
    on_body,
    on_message_complete,
    on_chunk_header,
    on_chunk_complete
};


static void on_close(uv_handle_t* handle) {
    http_connection *conn = CONTAINER_OF(handle, http_connection, handle);
    if (conn->recv_buf) {
        free(conn->recv_buf);
    }
    if (conn->send_buf) {
        free(conn->send_buf);
    }
    YOU_LOG_DEBUG("conn: %p destroy", conn);
    // todo: free header node
    QUEUE *q;
    struct http_header_field_value *head;
    while (!QUEUE_EMPTY(&conn->header.headers)) {
        q = QUEUE_HEAD(&conn->header.headers);
        head = QUEUE_DATA(q, struct http_header_field_value, node);
        QUEUE_REMOVE(q);
        free(head);
    }
    /*
    QUEUE_FOREACH(q, &conn->header.headers) {
        head = QUEUE_DATA(q, struct http_header_field_value, node);
        free(head);
    }
    */
    free(conn);
}

static void on_error(http_connection *conn, int err_code) {
    YOU_LOG_DEBUG("conn %p, error:%d", conn, err_code);
    conn->callbacking = 1;
    if (conn->settings.on_error) {
        conn->settings.on_error(conn, conn->user_data, err_code);
    }
    conn->callbacking = 0;
    if (conn->destroy) {
        free_http_connection(conn);
    }
}

static void on_read_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    http_connection *conn = CONTAINER_OF(handle, http_connection, handle);
    YOU_LOG_DEBUG("conn: %p", conn);
    ASSERT(conn->state >= s_connected && conn->state < s_closing);
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
    conn->recv_buf = buf->base;
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    http_connection *conn = CONTAINER_OF(stream, http_connection, handle.stream);
    YOU_LOG_DEBUG("conn: %p", conn);
    ASSERT(conn->state >= s_connected && conn->state < s_closing);
    
    conn->callbacking = 1;
    CHECK(nread == http_parser_execute(&conn->header.parser, &parser_setting, buf->base, nread));
    conn->callbacking = 0;

    free(conn->recv_buf);
    conn->recv_buf = NULL;
    
    if (conn->destroy) {
        free_http_connection(conn);
    }
}

static void on_write_done(uv_write_t *req, int status) {
    http_connection *conn = CONTAINER_OF(req, http_connection, write_req);
    YOU_LOG_DEBUG("conn: %p, status:%d", conn, status);
    if (status != 0) return on_error(conn, status);
    ASSERT(conn->state >= s_connected && conn->state < s_closing);
    free(conn->send_buf);
    conn->send_buf = NULL;
    if (conn->settings.on_send) {
        conn->settings.on_send(conn, conn->user_data);
    }
}

static void on_connect(uv_connect_t* req, int status) {
    http_connection *conn = CONTAINER_OF(req, http_connection, t.connect_req);
    YOU_LOG_DEBUG("conn: %p, status:%d", conn, status);
    if (status != 0) return on_error(conn, status);
    ASSERT(conn->state == s_connecting);
    conn->state = s_connected;
    uv_read_start(&conn->handle.stream, on_read_alloc, on_read);
    if (conn->settings.on_connect) {
        conn->settings.on_connect(conn, conn->user_data);
    }
}

static void on_get_addrinfo(uv_getaddrinfo_t *req, int status, struct addrinfo *addrs) {
    http_connection *conn = CONTAINER_OF(req, http_connection, t.addrinfo_req);
    YOU_LOG_DEBUG("conn: %p, status:%d", conn, status);
    ASSERT(conn->state == s_connecting);
    if (status == 0) {
        /* todo: FIXME(bnoordhuis) Should try all addresses. */
        if (addrs->ai_family == AF_INET) {
            conn->t.addr4 = *(const struct sockaddr_in *) addrs->ai_addr;
        } else if (addrs->ai_family == AF_INET6) {
            conn->t.addr6 = *(const struct sockaddr_in6 *) addrs->ai_addr;
        } else {
            UNREACHABLE();
        }
    } else {
        uv_freeaddrinfo(addrs);
        return on_error(conn, status);
    }
    
    uv_freeaddrinfo(addrs);
    
    uv_tcp_init(conn->loop, &conn->handle.tcp);
    uv_tcp_connect(&conn->t.connect_req, &conn->handle.tcp, &conn->t.addr, on_connect);
}

static http_connection* malloc_http_connection(uv_loop_t *loop, struct http_connection_settings *settings, void *user_data, enum http_parser_type t) {
    http_connection *conn = (http_connection*)malloc(sizeof(http_connection));
    memset(conn, 0, sizeof(http_connection));
    conn->user_data = user_data;
    conn->state = s_idle;
    conn->settings = *settings;
    http_parser_init(&conn->header.parser, t);
    conn->header.parser.data = conn;
    QUEUE_INIT(&conn->header.headers);
    
    conn->loop = loop;
    
//    CHECK(0 == uv_timer_init(loop, &conn->timer_handle));
    return conn;
}

http_connection* create_http_connection(uv_loop_t *loop, struct http_connection_settings settings, void *user_data) {
    http_connection* conn = malloc_http_connection(loop, &settings, user_data, HTTP_RESPONSE);
    YOU_LOG_DEBUG("conn: %p", conn);
    return conn;
}

http_connection* create_passive_http_connection(uv_stream_t *server, struct http_connection_settings settings, void *user_data) {
    http_connection *conn = malloc_http_connection(server->loop, &settings, user_data, HTTP_REQUEST);
    conn->state = s_connected;
    CHECK(0 == uv_tcp_init(server->loop, &conn->handle.tcp));
    CHECK(0 == uv_accept(server, &conn->handle.stream));
    uv_read_start(&conn->handle.stream, on_read_alloc, on_read);
    YOU_LOG_DEBUG("conn: %p", conn);
    return conn;
}

int http_connection_connect(http_connection *conn, const char host[MAX_HOST_LEN], unsigned short port) {

    conn->state = s_connecting;
    
    struct addrinfo hints;
    int err;
    
    /* Resolve the address of the interface that we should bind to.
     * The getaddrinfo callback starts the server and everything else.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    char port_str[16] = {0};
    snprintf(port_str, 16, "%u", port);
    YOU_LOG_DEBUG("conn: %p, %s:%u", conn, host, port);
    err = uv_getaddrinfo(conn->loop, &conn->t.addrinfo_req, on_get_addrinfo, host, port_str, &hints);
    return err;
}

int http_connection_send(http_connection *conn, const char *buf, size_t len) {
    YOU_LOG_DEBUG("conn: %p", conn);
    ASSERT(conn->send_buf == NULL);
    conn->send_buf = (char*)malloc(len);
    memcpy(conn->send_buf, buf, len);
    uv_buf_t b;
    b.base = conn->send_buf;
    b.len = len;
    uv_write(&conn->write_req, &conn->handle.stream, &b, 1, on_write_done);
    return 0;
}

void free_http_connection(http_connection *conn) {
    if (conn->callbacking) {
        YOU_LOG_DEBUG("conn: %p callbacking", conn);
        conn->destroy = 1;
        return;
    }
    YOU_LOG_DEBUG("conn: %p close", conn);
    conn->state = s_closing;
    uv_close(&conn->handle.handle, on_close);
    // todo: check if free all
}

void http_connection_stop_read(http_connection *conn) {
    YOU_LOG_DEBUG("conn: %p", conn);
    uv_read_stop(&conn->handle.stream);
}

void http_connecton_start_read(http_connection *conn) {
    YOU_LOG_DEBUG("conn: %p", conn);
    uv_read_start(&conn->handle.stream, on_read_alloc, on_read);
}

