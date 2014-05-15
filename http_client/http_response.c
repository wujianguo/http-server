#include "http_response.h"

uint64_t http_response_range_filesize(HTTP_RESPONSE_OBJ* resp)
{
	int i;
	for(i=0; i<resp->header_num; ++i)
	{
		if(memcmp(resp->headers[i].field, "Content-Range", strlen("Content-Range"))==0)
		{
			char* slash = strrchr(resp->headers[i].value, '/');	
			return atol(slash+1);
		}
	}
	return 0;
}