/******************************************************************************

            版权所有 (C), 2017-2018, xxx Co.xxx, Ltd.

 ******************************************************************************
    文 件 名 : ss_msg.c
    版 本 号 : V1.0
    作    者 : zc
    生成日期 : 2018年8月2日
    功能描述 : 消息处理
    修改历史 :
******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ss_msg.h"

typedef int (*ss_msg_pf)(int type, int len, char *data, ss_msg *rsp);

/* 事件注册函数 */
static ss_msg_pf ss_msg_func_arr[SS_MSG_TYPE_MAX];

/*****************************************************************************
    函 数 名 : ss_msg_deal_func
    功能描述 : 处理消息
    输入参数 : int type
               int len
               char *data
    输出参数 : ss_msg *rsp
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_msg_deal_func(int type, int len, char *data, ss_msg *rsp)
{
    int res = 1;

    const char *t_msg = SS_MSG_BY_TYPE_OF_INFO(type);
    printf("type = %d(%s), len = %d\n", type,
        (t_msg ? t_msg : "null"), len);

    switch (type) {
    case SS_TEST_MSG_NUM:
        snprintf(rsp->data, SS_BUFER_MAX - sizeof(ss_msg), SS_TEST_MSG);
        rsp->len = sizeof(SS_TEST_MSG) - 1;
        break;

    default:
        res = -1;
        break;
    }

    return res;
}

/*****************************************************************************
    函 数 名 : ss_msg_register
    功能描述 : 消息注册
    输入参数 : ss_msg_pf func
               int type
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_msg_register(ss_msg_pf func, int type)
{
    if ((type < SS_MSG_TYPE_MIN) ||
        (type >= SS_MSG_TYPE_MAX)) {
        printf("nm msg type(%d) error.\n", type);
        return;
    }

    if (func != NULL) {
        ss_msg_func_arr[type] = func;
    }

    return;
}

/*****************************************************************************
    函 数 名 : ss_msg_init
    功能描述 : 消息初始化
    输入参数 : 无
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
void ss_msg_init(void)
{
    int i;

    for (i = 0; i < SS_MSG_TYPE_MAX; i++) {
        ss_msg_register(NULL, i);
    }

    ss_msg_register(ss_msg_deal_func, SS_TEST_MSG_NUM);
}

/*****************************************************************************
    函 数 名 : ss_msg_deal
    功能描述 : 处理消息
    输入参数 : ss_msg *req
    输出参数 : ss_msg *resp
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
void ss_msg_deal(ss_msg *req, ss_msg *rsp)
{
    int ret = -1;

    if ((req->type >= SS_MSG_TYPE_MIN) &&
        (req->type < SS_MSG_TYPE_MAX)) {
        if (ss_msg_func_arr[req->type] != NULL) {
            ret = ss_msg_func_arr[req->type](req->type,
                req->len, req->data, rsp);
        }
    }
    rsp->type = SS_RESP_RESULT_NUM;
    rsp->result = ret;
}

