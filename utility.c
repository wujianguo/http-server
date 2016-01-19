//
//  utility.c
//  YouSirCmd
//
//  Created by 吴建国 on 15/12/31.
//  Copyright © 2015年 wujianguo. All rights reserved.
//

#include "utility.h"
#include "http_connection.h"
#include <stdlib.h>

/*
 *         support             not support
 *  ----------------------|------------------------------|
 *  Range: bytes=0-499    | Range: bytes=0-0,-1          |
 *  Range: bytes=500-999  | Range: bytes=500-600,601-999 |
 *  Range: bytes=-500     |                              |
 *  Range: bytes=500-     |                              |
 *
 *
 */
void parse_range(const char *buf, size_t buf_len, int64_t *pos, int64_t *end) {
    int index, pos_start = 0, pos_end = 0, len_start = 0;
    for (index = 0; index < buf_len; ++index) {
        if (buf[index] == '=') {
            pos_start = index + 1;
        } else if (buf[index] == '-') {
            pos_end = index;
            len_start = index + 1;
        }
    }
    ASSERT(pos_start <= pos_end);
    ASSERT(pos_end < len_start);
    ASSERT(buf_len >= len_start);
    char tmp[1024] = {0};
    if (pos_start == pos_end) {
        *pos = -1;
    } else {
        strncpy(tmp, buf + pos_start, pos_end - pos_start);
        *pos = atoll(tmp);
    }
    
    if (len_start == buf_len) {
        *end = -1;
    } else {
        strncpy(tmp, buf + len_start, buf_len - len_start);
        tmp[buf_len - len_start] = '\0';
        *end = atoll(tmp);
    }
}

/*
 // todo: manage test cases
 static void check_range_parser(const char *buf, int64_t pos, int64_t end) {
 int64_t ret_pos = 0, ret_end = 0;
 parse_range_imp(buf, strlen(buf), &ret_pos, &ret_end);
 ASSERT(ret_pos == pos);
 ASSERT(ret_end == end);
 }
 
 static void range_parser_test_case() {
 YOU_LOG_DEBUG("%lld", atoll("499"));
 check_range_parser("bytes=0-499", 0, 499);
 check_range_parser("bytes=500-999", 500, 999);
 check_range_parser("bytes=-500", -1, 500);
 check_range_parser("bytes=500-", 500, -1);
 }
 */


void get_query_argument(struct http_parser_url *url, char *url_buf, char *key, size_t key_len, size_t *off, size_t *len) {
    *off = 0;
    *len = 0;
    if (!(url->field_set & (1 << UF_QUERY)))
        return;

    uint16_t left_query_len = url->field_data[UF_QUERY].len;
    size_t cur_off = url->field_data[UF_QUERY].off;
    int query_start = 1;
    while (left_query_len > key_len) {
        if (url_buf[cur_off] == '&') {
            query_start = 1;
            cur_off++;
            left_query_len--;
            continue;
        }
        
        if (query_start && strncmp(url_buf + cur_off, key, key_len) == 0 && url_buf[cur_off + key_len] == '=') {
            *off = cur_off + key_len + 1;
            cur_off = *off;
            left_query_len--;
            break;
        }
        query_start = 0;
        cur_off++;
        left_query_len--;
    }
    if (*off == 0)
        return;

    while (left_query_len > 0 && url_buf[cur_off] != '&') {
        *len = *len + 1;
        left_query_len--;
        cur_off++;
    }
}

int is_response_content_encoding_gzip(struct http_header *header) {
    QUEUE *q = NULL;
    struct http_header_field_value *hd = NULL;
    QUEUE_FOREACH(q, &header->headers) {
        hd = QUEUE_DATA(q, struct http_header_field_value, node);
        if (strcmp(hd->field, "Content-Encoding")==0) {
            if (strcmp(hd->value, "gzip")==0) {
                return 1;
            }
            return 0;
        }
    }
    return 0;
}
