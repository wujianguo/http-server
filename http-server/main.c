//
//  main.c
//  http-server
//
//  Created by wujianguo on 16/1/1.
//  Copyright © 2016年 wujianguo. All rights reserved.
//

#include <stdio.h>
#include "uv.h"

int main(int argc, const char * argv[]) {
    // insert code here...
    printf("Hello, World!\n");
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return 0;
}
