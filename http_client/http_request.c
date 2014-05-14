#include <stdlib.h>
#include <stdio.h>
#include "http_client.h"
#include "http_parser/http_parser.h"


char* http_request_buf(HTTP_REQUEST_OBJ* req, unsigned int* len)
{
	struct http_parser_url url_obj = {0};
	http_parser_parse_url(req->url, strlen(req->url), 0, &url_obj);

	char* buf = (char*)malloc(1024);
	memset(buf, 0, 1024);
	snprintf(buf, 1024, "%s ", http_method_str(req->method));

	if(url_obj.field_set & (1 << UF_PATH))
	{
		strncat(buf, req->url+url_obj.field_data[UF_PATH].off, url_obj.field_data[UF_PATH].len);
	}
	else
	{
		strncat(buf, "/", 1);
	}
	strncat(buf, " HTTP/1.1\r\n", strlen(" HTTP/1.1\r\n"));
	int head_filed_host = 0;
	int head_filed_ua = 0;
	int i;
	for(i=0; i<req->header_num; ++i)
	{
		if(head_filed_host==0 && memcmp(req->headers[i].field, "Host", strlen("Host"))==0)
			head_filed_host = 1;
		if(head_filed_ua==0 && memcmp(req->headers[i].field, "User-Agent", strlen("User-Agent"))==0)
			head_filed_ua = 1;
		snprintf(buf+strlen(buf), 1024-strlen(buf), "%s:%s\r\n", req->headers[i].field, req->headers[i].value);
	}
	if(!head_filed_host)
	{
		snprintf(buf+strlen(buf), 1024-strlen(buf), "Host:");
		strncat(buf, req->url + url_obj.field_data[UF_HOST].off,url_obj.field_data[UF_HOST].len);
		if(!url_obj.port)
			url_obj.port = 80;
		snprintf(buf+strlen(buf), 1024-strlen(buf), ":%u\r\n",url_obj.port);
	}
	if(!head_filed_ua)
	{
		snprintf(buf+strlen(buf), 1024-strlen(buf), "User-Agent:Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/34.0.1847.131 Safari/537.36\r\n");
	}
	// snprintf(buf+strlen(buf),1024-strlen(buf), "Connection:keep-alive\r\n");
	snprintf(buf+strlen(buf),1024-strlen(buf), "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n");
	snprintf(buf+strlen(buf),1024-strlen(buf), "Accept-Encoding:gzip,deflate,sdch\r\n");
	snprintf(buf+strlen(buf),1024-strlen(buf), "Accept-Language:zh-CN,zh;q=0.8,en-US;q=0.6,en;q=0.4\r\n");

	strncat(buf, "\r\n", strlen("\r\n"));
	*len = strlen(buf)+1;
	return buf;
}