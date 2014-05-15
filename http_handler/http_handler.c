#include "http_handler.h"
#include "main_handler.h"
#include "not_found_handler.h"
#include <stdio.h>
#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

// #define APPLICATIONS_MAP(XX)			\
// 	XX(main_handler, "/")				\
// 	XX(play_handler, "/play")			\

typedef void (*handler_func)(struct _tagHttpResponseObj* resp);
static handler_func get_handler(const char* url)
{
	LOG_DEBUG("%s", url);
	if(memcmp(url, "/", strlen(url))==0)
		return handle_main;
	else
		return handle_not_found;
}

void handle(HTTP_RESPONSE_OBJ* http_resp)
{
	get_handler(http_resp->request->url)(http_resp);
}
