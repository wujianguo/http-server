#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uv.h"
#include "http_connection.h"
#include "http-parser/http_parser.h"
#include "queue.h"

#define LOG_DEBUG(format, ...) printf("\033[0;32;32m[%s:%d] " format "\n\033[m", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) printf("\033[0;32;31m[%s:%d] " format "\n\033[m", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define HTTP_ERR_WRONG_STATE -1


typedef struct tagHTTP_CONNECTION_BUFFER{
	char* _buffer;
	uint32_t  _buffer_length;
	QUEUE _node;
}HTTP_CONNECTION_BUFFER;

typedef enum tagHTTP_CONNECTION_STATUS{
	HTTP_CONNECTION_IDLE,
	HTTP_CONNECTION_CONNECTING,
	HTTP_CONNECTION_CONNECTED,
	HTTP_CONNECTION_ERROR,
	HTTP_CONNECTION_CLOSED
}HTTP_CONNECTION_STATUS;

typedef struct tagHTTP_CONNECTION{
	uv_tcp_t _sock;
	HTTP_CONNECTION_STATUS _status;
	HTTP_CONNECTION_CALLBACK _conn_callback;
	QUEUE _send_requests;
	uv_write_t _w_req;
	uv_getaddrinfo_t _resolver;
	uv_connect_t	_connection;
	http_parser		_parser;
	void* _user_data;
}HTTP_CONNECTION;

static int on_message_begin(http_parser* parser)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_message_begin)
		return http_connection->_conn_callback.on_message_begin(http_connection, http_connection->_user_data);
	else
		return SUCCESS;
}

static int on_url(http_parser* parser, const char *at, size_t length)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_url)
		return http_connection->_conn_callback.on_url(http_connection, http_connection->_user_data, at, length);
	else
		return SUCCESS;
}

static int on_status(http_parser* parser, const char *at, size_t length)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_status)
		return http_connection->_conn_callback.on_status(http_connection, http_connection->_user_data, at, length);
	else
		return SUCCESS;
}

static int on_header_field(http_parser* parser, const char *at, size_t length)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_header_field)
		return http_connection->_conn_callback.on_header_field(http_connection, http_connection->_user_data, at, length);
	else
		return SUCCESS;
}

static int on_header_value(http_parser* parser, const char *at, size_t length)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_header_value)
		return http_connection->_conn_callback.on_header_value(http_connection, http_connection->_user_data, at, length);
	else
		return SUCCESS;
}

static int on_header_complete(http_parser* parser)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_header_complete)
		return http_connection->_conn_callback.on_header_complete(http_connection, http_connection->_user_data);
	else
		return SUCCESS;
}
static int on_body(http_parser* parser, const char *at, size_t length)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_body)
		return http_connection->_conn_callback.on_body(http_connection, http_connection->_user_data, at, length);
	else
		return SUCCESS;
}

static int on_message_complete(http_parser* parser)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)parser->data;
	if(http_connection->_conn_callback.on_message_complete)
		return http_connection->_conn_callback.on_message_complete(http_connection, http_connection->_user_data);
	else
		return SUCCESS;
}

static const http_parser_settings g_parser_cb = {on_message_begin, on_url, on_status, on_header_field, on_header_value, on_header_complete, on_body, on_message_complete};

static void free_http_connection( HTTP_CONNECTION* http_connection )
{
	LOG_DEBUG("[http_connection = %p]", http_connection);
	while(!QUEUE_EMPTY(&http_connection->_send_requests))
	{
		HTTP_CONNECTION_BUFFER* send_buffer = QUEUE_DATA(QUEUE_HEAD(&http_connection->_send_requests), HTTP_CONNECTION_BUFFER, _node);
		assert(send_buffer!=NULL);
		assert(send_buffer->_buffer!=NULL);
		assert(send_buffer->_buffer_length!=0);
		LOG_DEBUG("[http_connection = %p] free buf: %p:%p", http_connection, send_buffer, send_buffer->_buffer);
		QUEUE_REMOVE(&send_buffer->_node);
		free(send_buffer->_buffer);
		free(send_buffer);
	}
	LOG_DEBUG("[http_connection = %p], finished", http_connection);
}

static void on_error( HTTP_CONNECTION* http_connection, int32_t error_code )
{
	LOG_ERROR("[http_connection = %p] error_code = %d", http_connection,error_code );
	http_connection->_status = HTTP_CONNECTION_ERROR;
	if ( http_connection->_conn_callback.on_network_error)
	{
		http_connection->_conn_callback.on_network_error( http_connection, http_connection->_user_data, error_code);
	}
}

static void on_close(uv_handle_t* handle)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)handle->data;
	LOG_DEBUG("[http_connection = %p]", http_connection);
	if(http_connection->_status == HTTP_CONNECTION_CLOSED || http_connection->_status == HTTP_CONNECTION_ERROR)
		free_http_connection(http_connection);
	else if(http_connection->_conn_callback.on_network_error)
		http_connection->_conn_callback.on_network_error(http_connection, http_connection->_user_data, -2);
}

static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)handle->data;
	buf->base = (char*)malloc(suggested_size);
	buf->len = suggested_size;
	LOG_DEBUG("[http_connection = %p] alloc:%p, %d", http_connection, buf->base, buf->len);
}

static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)stream->data;
	LOG_DEBUG("[http_connection = %p] nread:%d", http_connection, nread);
	if(http_connection->_status == HTTP_CONNECTION_CLOSED)
	{
		LOG_DEBUG("[http_connection = %p] _status = HTTP_CONNECTION_CLOSED", http_connection);
		free_http_connection(http_connection);
		LOG_DEBUG("[http_connection = %p] free:%p", http_connection, buf->base);
		free(buf->base);
		return;
	}
	if(nread<0)
	{
		LOG_DEBUG("[http_connection = %p] nread:%d", http_connection, nread);
		if(http_connection->_conn_callback.on_network_error)
			http_connection->_conn_callback.on_network_error(http_connection, http_connection->_user_data, nread);
		LOG_DEBUG("[http_connection = %p] free:%p", http_connection, buf->base);
		free(buf->base);
		return;
	}

	//**********************************************************
	http_parser_execute(&http_connection->_parser, &g_parser_cb, buf->base, nread);
	LOG_DEBUG("[http_connection = %p] free:%p", http_connection, buf->base);
	free(buf->base);
}

static void after_write(uv_write_t* req, int status)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)req->data;
	LOG_DEBUG("[http_connection = %p]", http_connection);
	if(http_connection->_status == HTTP_CONNECTION_CLOSED)
	{
		LOG_DEBUG("[http_connection = %p] _status = HTTP_CONNECTION_CLOSED", http_connection);
		free_http_connection(http_connection);
		return;
	}
	if(status<0)
	{
		LOG_ERROR("[http_connection = %p] %d", http_connection, status);
		on_error(http_connection, status);
		return;
	}
	HTTP_CONNECTION_BUFFER* send_buffer = QUEUE_DATA(QUEUE_HEAD(&http_connection->_send_requests), HTTP_CONNECTION_BUFFER, _node);
	assert(send_buffer!=NULL);
	assert(send_buffer->_buffer!=NULL);
	assert(send_buffer->_buffer_length!=0);
	uint32_t send_length = send_buffer->_buffer_length;
	LOG_DEBUG("[http_connection = %p] free buf: %p:%p", http_connection, send_buffer, send_buffer->_buffer);
	free(send_buffer->_buffer);
	QUEUE_REMOVE(&send_buffer->_node);
	free(send_buffer);
	if(!QUEUE_EMPTY(&http_connection->_send_requests))
	{
		send_buffer = QUEUE_DATA(QUEUE_HEAD(&http_connection->_send_requests), HTTP_CONNECTION_BUFFER, _node);
		http_connection->_w_req.data = http_connection;
		uv_buf_t uv_buf = uv_buf_init(send_buffer->_buffer, send_buffer->_buffer_length);
		int32_t result = uv_write(&http_connection->_w_req, (uv_stream_t*)&http_connection->_sock, &uv_buf, 1, after_write);
		if(result!=SUCCESS)
		{
			LOG_ERROR("[http_connection = %p] %d", http_connection, result);
			on_error(http_connection, result);
			return;
		}
	}
	if(http_connection->_conn_callback.on_send)
		http_connection->_conn_callback.on_send(http_connection, http_connection->_user_data, send_length);
}

static void on_connect(uv_connect_t* req, int status)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)req->data;
	LOG_DEBUG("[http_connection = %p]", http_connection);
	if(http_connection->_status == HTTP_CONNECTION_CLOSED)
	{
		LOG_DEBUG("[http_connection = %p] _status = HTTP_CONNECTION_CLOSED", http_connection);
		free_http_connection(http_connection);
		return;
	}
	if(status<0)
	{
		LOG_ERROR("[http_connection = %p] %d", http_connection, status);
		on_error(http_connection, status);
		return;
	}
	assert(http_connection->_status == HTTP_CONNECTION_CONNECTING );
	http_connection->_parser.data = http_connection;
	http_parser_init(&http_connection->_parser, HTTP_RESPONSE);
	int32_t result = uv_read_start((uv_stream_t*)&http_connection->_sock, on_alloc, on_read);

	if ( result != SUCCESS )
	{
		LOG_ERROR("[http_connection = %p] %d", http_connection, status);
		on_error( http_connection, result );
		return;
	}

	http_connection->_status = HTTP_CONNECTION_CONNECTED;
	if(http_connection->_conn_callback.on_connect_succ)
	{
		http_connection->_conn_callback.on_connect_succ(http_connection, http_connection->_user_data);
	}
}

static void on_resolved(uv_getaddrinfo_t *r, int status, struct addrinfo *res)
{
	HTTP_CONNECTION* http_connection = (HTTP_CONNECTION*)r->data;
	LOG_DEBUG("[http_connection = %p]", http_connection);
	if(http_connection->_status == HTTP_CONNECTION_CLOSED)
	{
		LOG_DEBUG("[http_connection = %p] _status = HTTP_CONNECTION_CLOSED", http_connection);
		free_http_connection(http_connection);
		return;
	}
	if(status<0)
	{
		LOG_ERROR("[http_connection = %p] %d", http_connection, status);
		on_error(http_connection, status);
		return;
	}
	http_connection->_connection.data = http_connection;
	uv_tcp_connect(&http_connection->_connection, &http_connection->_sock, (struct sockaddr*)res->ai_addr, on_connect);
}

int32_t http_connection_connect( HTTP_CONNECTION* http_connection, char* host, uint16_t port )
{
	if( http_connection->_status != HTTP_CONNECTION_IDLE )
	{
		LOG_DEBUG("[http_connection = %p] state not valid, state = %d", http_connection, http_connection->_status);
		return SUCCESS;
	}

	LOG_DEBUG("[http_connection = %p] host=%s, port=%d.", http_connection, host, port );

	int32_t result = SUCCESS;
	http_connection->_status = HTTP_CONNECTION_CONNECTING;
	http_connection->_resolver.data = http_connection;
	char p[10] = {0};
	snprintf(p, 10, "%u", port);
	result = uv_getaddrinfo(uv_default_loop(), &http_connection->_resolver, on_resolved, host, p, NULL);
	return result;
}

int32_t http_connection_send( HTTP_CONNECTION* http_connection, char* request_buf, uint32_t request_len )
{
	if (http_connection->_status != HTTP_CONNECTION_CONNECTED )
	{
		LOG_ERROR("[http_connection = %p] state not valid, state = %d", http_connection, http_connection->_status);
		return HTTP_ERR_WRONG_STATE;
	}

	LOG_DEBUG("[http_connection = %p] request_len=%d, status=%d.", http_connection,request_len,http_connection->_status);	

	HTTP_CONNECTION_BUFFER* send_buffer = malloc(sizeof(HTTP_CONNECTION_BUFFER));
	send_buffer->_buffer = malloc(request_len);
	LOG_DEBUG("[http_connection = %p] malloc buf: %p:%p", http_connection, send_buffer, send_buffer->_buffer);
	memcpy(send_buffer->_buffer, request_buf, request_len);
	send_buffer->_buffer_length = request_len;
	QUEUE_INIT(&send_buffer->_node);
	int32_t result = SUCCESS;
	if(QUEUE_EMPTY(&http_connection->_send_requests))
	{
		http_connection->_w_req.data = http_connection;
		uv_buf_t uv_buf = uv_buf_init(send_buffer->_buffer, send_buffer->_buffer_length);
		result = uv_write(&http_connection->_w_req, (uv_stream_t*)&http_connection->_sock, &uv_buf, 1, after_write);
	}
	QUEUE_INSERT_TAIL(&http_connection->_send_requests, &send_buffer->_node);	
	return result;
}


int32_t http_connection_destory( HTTP_CONNECTION* http_connection )
{
	LOG_DEBUG("[http_connection = %p]", http_connection );
	assert(http_connection->_status != HTTP_CONNECTION_CLOSED);
	http_connection->_status = HTTP_CONNECTION_CLOSED;
	if(http_connection->_status!=HTTP_CONNECTION_IDLE)
	{
		uv_close((uv_handle_t*)&http_connection->_sock, on_close);
		return SUCCESS;
	}
	free_http_connection( http_connection );
	return SUCCESS;
}

int32_t http_connection_create(HTTP_CONNECTION** conn ,HTTP_CONNECTION_CALLBACK conn_callback, void* user_data)
{
	HTTP_CONNECTION* http_connection = malloc(sizeof(HTTP_CONNECTION));
	memset(http_connection, 0, sizeof(HTTP_CONNECTION));
	uv_tcp_init(uv_default_loop(), &http_connection->_sock);
	http_connection->_sock.data = http_connection;
	http_connection->_conn_callback = conn_callback;
	http_connection->_user_data = user_data;
	QUEUE_INIT(&http_connection->_send_requests);
	http_connection->_status = HTTP_CONNECTION_IDLE;
	
	*conn = http_connection;
    
	LOG_DEBUG("[http_connection = %p] user_data=%p", http_connection, http_connection->_user_data );
	return SUCCESS;
}

int32_t http_connection_passive_create(HTTP_CONNECTION** conn, uv_stream_t* server, HTTP_CONNECTION_CALLBACK conn_callback, void* user_data)
{
	HTTP_CONNECTION* http_connection = malloc(sizeof(HTTP_CONNECTION));
	memset(http_connection, 0, sizeof(HTTP_CONNECTION));
	uv_tcp_init(uv_default_loop(), &http_connection->_sock);
	http_connection->_sock.data = http_connection;
	http_connection->_conn_callback = conn_callback;
	http_connection->_user_data = user_data;
	QUEUE_INIT(&http_connection->_send_requests);	
	http_connection->_status = HTTP_CONNECTION_CONNECTED;

	LOG_DEBUG("[http_connection = %p]", http_connection );
	int32_t result = uv_accept(server, (uv_stream_t*)&http_connection->_sock);
	if(result!=SUCCESS)
	{
		LOG_DEBUG("[http_connection = %p] %d", http_connection, result);
		free_http_connection(http_connection);
		return result;
	}
	http_connection->_parser.data = http_connection;
	http_parser_init(&http_connection->_parser, HTTP_REQUEST);
	result = uv_read_start((uv_stream_t*)&http_connection->_sock, on_alloc, on_read);
	if(result != SUCCESS)
	{
		LOG_DEBUG("[http_connection = %p] %d", http_connection, result);
		free_http_connection(http_connection);
		return result;
	}
	
	*conn = http_connection;
	LOG_DEBUG("[http_connection = %p] user_data=%p", http_connection, http_connection->_user_data );
	
	return SUCCESS;
}
