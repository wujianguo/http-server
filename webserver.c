#include <stdio.h>
#include <string.h>
#include "uv.h"
#include "http_connection.h"

#define SERVER_IP "0.0.0.0"
#define SERVER_PORT 8922

#define LOG_DEBUG(format, ...) printf("\033[0;32;32m[%s:%d] " format "\n\033[m", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) printf("\033[0;32;31m[%s:%d] " format "\n\033[m", __FUNCTION__, __LINE__, ##__VA_ARGS__)

static uv_tcp_t server;

static int on_connect_succ( HTTP_CONNECTION* conn, void* user_data)
{
	LOG_DEBUG("user:%d", (int)user_data);
	return SUCCESS;
}

static int on_network_error( HTTP_CONNECTION* conn, void* user_data, int error_code)
{
	LOG_DEBUG("user:%d", (int)user_data);
	http_connection_destory(conn);
	return SUCCESS;
}

static int on_send( HTTP_CONNECTION* conn, void* user_data, uint32_t len)
{
	LOG_DEBUG("user:%d, len:%d", (int)user_data, len);
	http_connection_destory(conn);
	return SUCCESS;
}

static int on_message_complete( HTTP_CONNECTION* conn, void* user_data)
{
	LOG_DEBUG("user:%d", (int)user_data);
	static const char* buf = "HTTP/1.1 200 OK\r\n"  
		"Content-Type:text/html;charset=utf-8\r\n"  
		"Content-Length:12\r\n"  
		"\r\n"  
		"Hello World!";
	http_connection_send(conn, (char*)buf, strlen(buf));
	return SUCCESS;
}

static void on_connect(uv_stream_t* server_handle, int status) 
{
	if(status!=0)
		return;
	HTTP_CONNECTION* conn = NULL;
	HTTP_CONNECTION_CALLBACK conn_callback = {on_connect_succ, on_send, NULL, NULL, NULL, NULL, NULL, NULL, NULL, on_message_complete, on_network_error};
	static int i=0;
	i++;
	http_connection_passive_create(&conn, server_handle, conn_callback, (void*)i);
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
    start_server();
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}
