#include "http_handler.h"
#include "main_handler.h"
#include "not_found_handler.h"
#include "play_handler.h"

#define APPLICATIONS_MAP(XX)			\
	XX(handle_main, "/")				\
	XX(handle_play, "/play")			\

#include <stdio.h>
#define LOG_DEBUG(format, ...) printf("[%s:%d] " format "\n",__FUNCTION__, __LINE__,##__VA_ARGS__)

typedef void (*handler_func)(struct _tagHttpResponseObj* resp);

#define GET_HANDLER_GEN(handler, path) if(memcmp(url+u.field_data[UF_PATH].off, path, u.field_data[UF_PATH].len)==0)return handler;
static handler_func get_handler(const char* url)
{
	LOG_DEBUG("%s", url);
	struct http_parser_url u = {0};
	http_parser_parse_url(url, strlen(url), 0, &u);
	if(u.field_set & (1 << UF_PATH))
	{
		APPLICATIONS_MAP(GET_HANDLER_GEN)
	}
	return handle_not_found;
}
#undef GET_HANDLER_GEN

void handle(HTTP_RESPONSE_OBJ* http_resp)
{
	get_handler(http_resp->request->url)(http_resp);
}
