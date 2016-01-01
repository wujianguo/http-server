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