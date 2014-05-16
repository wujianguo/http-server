#ifndef _APPLICATIONS_H_
#define _APPLICATIONS_H_


#define APPLICATIONS_MAP(XX)			\
	XX(handle_main, "/")				\
	XX(handle_play, "/play")			\

#include "http_client/http_request.h"

#define GEN_HANDLERS_FUNC_DECLARE(h, path) void h(HTTP_RESPONSE_OBJ* http_resp);
	APPLICATIONS_MAP(GEN_HANDLERS_FUNC_DECLARE)
	void handle_not_found(HTTP_RESPONSE_OBJ* http_resp);
#undef GEN_HANDLERS_FUNC_DECLARE


#endif