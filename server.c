/******************************************************************************

            版权所有 (C), 2017-2018, xxx Co.xxx, Ltd.

 ******************************************************************************
    文 件 名 : server.c
    版 本 号 : V1.0
    作    者 : zc
    生成日期 : 2018年8月2日
    功能描述 : 测试代码server
    修改历史 :
******************************************************************************/
#include <stdio.h>

#include "ss_epoll.h"

int main(int argc, const char *argv[])
{
    int ret = -1;

    ss_msg_init();

    ret = ss_init_server();
    SS_RETURN_RES(ret < 0, -1);

    while (1) {
        ss_process_events();        
    }

    return 0;
}

