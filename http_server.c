//
//  http_server.c
//  YouSirCmd
//
//  Created by 吴建国 on 15/12/29.
//  Copyright © 2015年 wujianguo. All rights reserved.
//

#include <stdlib.h>
#include "http_server.h"


const static char not_found_response[] = "HTTP/1.1 404 Not Found\r\n"
"Server: YouSir/2\r\n"
"Content-type: text/plain\r\n"
"Content-Length: 10\r\n"
"\r\n"
"not found\n";

static void not_found_handler_on_header_complete(http_request *req) {
    
}

static void not_found_handler_on_body(http_request *req, const char *at, size_t length) {
    
}

static void not_found_handler_on_message_complete(http_request *req) {
    http_connection_send(req->conn, not_found_response, strlen(not_found_response));
}

static void not_found_handler_on_send(http_request *req) {
    req->complete(req);
}

http_handler_setting not_found_handler = {
    not_found_handler_on_header_complete,
    not_found_handler_on_body,
    not_found_handler_on_message_complete,
    not_found_handler_on_send,
    {0},
    {0}
};


typedef struct {
    unsigned int idle_timeout;  /* Connection idle timeout in ms. */
    uv_tcp_t tcp_handle;
    uv_loop_t *loop;
    http_server_config *config;
} http_server_ctx;

typedef struct {
    uv_getaddrinfo_t getaddrinfo_req;
    http_server_config *config;
    http_server_ctx *servers;
    uv_loop_t *loop;
} http_server_state;


typedef struct {
    http_request req;
    http_handler_setting *handler;
    http_server_ctx *sx;
} http_request_imp;


static http_handler_setting* find_handler(const char *url_buf, struct http_parser_url *url, http_server_config *config) {
    if (!(url->field_set & (1 << UF_PATH)))
        return &not_found_handler;
    
    http_handler_setting *handler;
    QUEUE *q;
    QUEUE_FOREACH(q, &config->handlers) {
        handler = QUEUE_DATA(q, http_handler_setting, node);
        
        size_t path_len = strlen(handler->path);
        if (path_len != url->field_data[UF_PATH].len)
            continue;
        
        if (strncmp(url_buf + url->field_data[UF_PATH].off, handler->path, path_len) == 0)
            return handler;
    }
    return &not_found_handler;
}

static void on_send(http_connection *conn, void *user_data) {
    http_request_imp *imp = (http_request_imp*)user_data;
    ASSERT(imp->handler != NULL);
    if (imp->handler->on_send) {
        imp->handler->on_send(&imp->req);
    }
}

static void on_header_complete(http_connection *conn, struct http_header *header, void *user_data) {
    http_request_imp *imp = (http_request_imp*)user_data;
    imp->req.header = header;
    imp->handler = find_handler(header->url_buf, &header->url, imp->sx->config);
    if (imp->handler->on_header_complete) {
        imp->handler->on_header_complete(&imp->req);
    }
}

static void on_body(http_connection *conn, const char *at, size_t length, void *user_data) {
    http_request_imp *imp = (http_request_imp*)user_data;
    ASSERT(imp->handler != NULL);
    if (imp->handler->on_body) {
        imp->handler->on_body(&imp->req, at, length);
    }
}

static void on_message_complete(http_connection *conn, void *user_data) {
    http_request_imp *imp = (http_request_imp*)user_data;
    ASSERT(imp->handler != NULL);
    if (imp->handler->on_message_complete) {
        imp->handler->on_message_complete(&imp->req);
    }
}

static struct http_connection_settings settings = {
    NULL,
    on_send,
    on_header_complete,
    on_body,
    on_message_complete
};

static void on_sess_complete(http_request *req) {
    free_http_connection(req->conn);
    http_request_imp *imp = (http_request_imp*)req;
    free(imp);
}

static void on_connection(uv_stream_t *server, int status) {
    CHECK(status == 0);

    http_server_ctx *sx = CONTAINER_OF(server, http_server_ctx, tcp_handle);
    
    http_request_imp *imp = (http_request_imp*)malloc(sizeof(http_request_imp));
    memset(imp, 0, sizeof(http_request_imp));
    imp->sx = sx;
    imp->req.conn = create_passive_http_connection(server, settings, imp);
    imp->req.complete = on_sess_complete;
    imp->req.loop = server->loop;
}

/* Bind a server to each address that getaddrinfo() reported. */
static void do_bind(uv_getaddrinfo_t *req, int status, struct addrinfo *addrs) {
    char addrbuf[INET6_ADDRSTRLEN + 1];
    unsigned int ipv4_naddrs;
    unsigned int ipv6_naddrs;
    http_server_state *state;
    http_server_config *cf;
    struct addrinfo *ai;
    const void *addrv;
    const char *what;
    uv_loop_t *loop;
    http_server_ctx *sx;
    unsigned int n;
    int err;
    union {
        struct sockaddr addr;
        struct sockaddr_in addr4;
        struct sockaddr_in6 addr6;
    } s;
    
    state = CONTAINER_OF(req, http_server_state, getaddrinfo_req);
    loop = state->loop;
    cf = state->config;
    
    if (status < 0) {
        YOU_LOG_ERROR("getaddrinfo(\"%s\"): %s", cf->bind_host, uv_strerror(status));
        uv_freeaddrinfo(addrs);
        return;
    }
    
    ipv4_naddrs = 0;
    ipv6_naddrs = 0;
    for (ai = addrs; ai != NULL; ai = ai->ai_next) {
        if (ai->ai_family == AF_INET) {
            ipv4_naddrs += 1;
        } else if (ai->ai_family == AF_INET6) {
            ipv6_naddrs += 1;
        }
    }
    
    if (ipv4_naddrs == 0 && ipv6_naddrs == 0) {
        YOU_LOG_ERROR("%s has no IPv4/6 addresses", cf->bind_host);
        uv_freeaddrinfo(addrs);
        return;
    }
    
    state->servers = malloc((ipv4_naddrs + ipv6_naddrs) * sizeof(state->servers[0]));
    
    n = 0;
    for (ai = addrs; ai != NULL; ai = ai->ai_next) {
        if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6) {
            continue;
        }
        
        if (ai->ai_family == AF_INET) {
            s.addr4 = *(const struct sockaddr_in *) ai->ai_addr;
            s.addr4.sin_port = htons(cf->bind_port);
            addrv = &s.addr4.sin_addr;
        } else if (ai->ai_family == AF_INET6) {
            s.addr6 = *(const struct sockaddr_in6 *) ai->ai_addr;
            s.addr6.sin6_port = htons(cf->bind_port);
            addrv = &s.addr6.sin6_addr;
        } else {
            UNREACHABLE();
        }
        
        if (uv_inet_ntop(s.addr.sa_family, addrv, addrbuf, sizeof(addrbuf))) {
            UNREACHABLE();
        }
        
        sx = state->servers + n;
        sx->loop = loop;
        sx->idle_timeout = state->config->idle_timeout;
        sx->config = state->config;
        CHECK(0 == uv_tcp_init(loop, &sx->tcp_handle));
        
        what = "uv_tcp_bind";
        err = uv_tcp_bind(&sx->tcp_handle, &s.addr, 0);
        if (err == 0) {
            what = "uv_listen";
            err = uv_listen((uv_stream_t *) &sx->tcp_handle, 128, on_connection);
        }
        
        if (err != 0) {
            YOU_LOG_ERROR("%s(\"%s:%hu\"): %s",
                   what,
                   addrbuf,
                   cf->bind_port,
                   uv_strerror(err));
            while (n > 0) {
                n -= 1;
                uv_close((uv_handle_t *) (state->servers + n), NULL);
            }
            break;
        }
        
        YOU_LOG_INFO("listening on %s:%hu", addrbuf, cf->bind_port);
        n += 1;
    }
    
    uv_freeaddrinfo(addrs);
}


int http_server_run(const http_server_config *cf, uv_loop_t *loop) {
    
    struct addrinfo hints;
    http_server_state state;
    int err;
    
    memset(&state, 0, sizeof(state));
    state.servers = NULL;
    state.config = (http_server_config*)cf;
    state.loop = loop;
    
    /* Resolve the address of the interface that we should bind to.
     * The getaddrinfo callback starts the server and everything else.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    err = uv_getaddrinfo(loop,
                         &state.getaddrinfo_req,
                         do_bind,
                         cf->bind_host,
                         NULL,
                         &hints);
    if (err != 0) {
        YOU_LOG_ERROR("getaddrinfo: %s", uv_strerror(err));
        return err;
    }
    
    /* Start the event loop.  Control continues in do_bind(). */
    if (uv_run(loop, UV_RUN_DEFAULT)) {
        abort();
    }
    
    /* Please Valgrind. */
    uv_loop_delete(loop);
    free(state.servers);
    return 0;
}