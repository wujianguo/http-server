#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include "http_client/http_request.h"

HTTP_RESPONSE_OBJ* http_client_request(HTTP_REQUEST_OBJ* req);

void http_client_cancel(HTTP_RESPONSE_OBJ* resp);

#endif