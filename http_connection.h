#ifndef download_engine_http_connection_h
#define download_engine_http_connection_h

#ifdef __cplusplus
extern "C" 
{
#endif

#define SUCCESS 0

typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct tagHTTP_CONNECTION HTTP_CONNECTION;

typedef struct tagHTTP_CONNECTION_CALLBACK
{
	int (*on_connect_succ)( HTTP_CONNECTION* conn, void* user_data );
	int (*on_send)( HTTP_CONNECTION* conn, void* user_data, uint32_t len);
	int (*on_message_begin)( HTTP_CONNECTION* conn, void* user_data);
	int (*on_url)( HTTP_CONNECTION* conn, void* user_data, const char* at, size_t length);
	int (*on_status)( HTTP_CONNECTION* conn, void* user_data, const char* at, size_t length);
	int (*on_header_field)( HTTP_CONNECTION* conn, void* user_data, const char* at, size_t length);
	int (*on_header_value)( HTTP_CONNECTION* conn, void* user_data, const char* at, size_t length);
	int (*on_header_complete)( HTTP_CONNECTION* conn, void* user_data);
	int (*on_body)( HTTP_CONNECTION* conn, void* user_data, const char* at, size_t length);
	int (*on_message_complete)( HTTP_CONNECTION* conn, void* user_data);
	int (*on_network_error)( HTTP_CONNECTION* conn, void* user_data, int error_code);
}HTTP_CONNECTION_CALLBACK;

int32_t http_connection_create(HTTP_CONNECTION** conn, HTTP_CONNECTION_CALLBACK conn_callback, void* user_data);

int32_t http_connection_passive_create(HTTP_CONNECTION** conn, uv_stream_t* server, HTTP_CONNECTION_CALLBACK conn_callback, void* user_data);

int32_t http_connection_connect( HTTP_CONNECTION* conn, char* host, uint16_t port );

int32_t http_connection_send( HTTP_CONNECTION* conn, char* request_buf, uint32_t request_len );

int32_t http_connection_destory( HTTP_CONNECTION* conn );

#ifdef __cplusplus
}
#endif


#endif

