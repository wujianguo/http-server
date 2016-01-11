//
//  utility.h
//  YouSirCmd
//
//  Created by 吴建国 on 15/12/31.
//  Copyright © 2015年 wujianguo. All rights reserved.
//

#ifndef utility_h
#define utility_h
#include "libuv/include/uv.h"
#include "http-parser/http_parser.h"
#include "http_connection.h"
void parse_range(const char *buf, size_t buf_len, int64_t *pos, int64_t *end);
void get_query_argument(struct http_parser_url *url, char *url_buf, char *key, size_t key_len, size_t *off, size_t *len);
int is_response_content_encoding_gzip(struct http_header *header);

#endif /* utility_h */
