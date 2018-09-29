/******************************************************************************

            版权所有 (C), 2017-2018, xxx Co.xxx, Ltd.

 ******************************************************************************
    文 件 名 : client.c
    版 本 号 : V1.0
    作    者 : zc
    生成日期 : 2018年8月2日
    功能描述 : 测试代码client
    修改历史 :
******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ss_epoll.h"

/*****************************************************************************
    函 数 名 : ss_usage
    功能描述 : 打印帮助信息
    输入参数 : 无
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月3日
*****************************************************************************/
static void ss_usage(void)
{
    int t;

    printf("Usage:\n");
    printf("  client [-t <type>]\n");
    printf("---------------------------------------------\n");
    printf("  [type]       [info]\n");
    for (t = SS_MSG_TYPE_MIN; t < SS_MSG_TYPE_MAX; t++) {
        const char *msg = SS_MSG_BY_TYPE_OF_INFO(t);
        if (msg) printf("   %d            %s\n", t, msg);
    }
    printf("---------------------------------------------\n");
}

/*****************************************************************************
    函 数 名 : ss_check_type
    功能描述 : 检查type合法性
    输入参数 : int type
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月3日
*****************************************************************************/
static int ss_check_type(int type)
{
    if (type < 0 || type >= SS_MSG_TYPE_MAX) {
        printf("invalid msg type(%d)\n", type);
        return -1;
    }

    return type;
}

/*****************************************************************************
    函 数 名 : ss_socket
    功能描述 : 创建socket
    输入参数 : 无
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月3日
*****************************************************************************/
static int ss_socket(void)
{
    int fd = -1;
    struct sockaddr_un sa;
    struct timeval timeout = {3, 0};

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    snprintf(sa.sun_path, sizeof(sa.sun_path), SS_UNIX_SOCKET);
    if (access(sa.sun_path, F_OK) < 0) {
        printf("unix socket file(%s) not exist\n", sa.sun_path);
        goto err;
    }

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        printf("create socket error: %s\n", strerror(errno));
        goto err;
    }

    /* 设置接收超时时间 */
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
        (const char*)&timeout,sizeof(timeout)) < 0) {
        printf("setsockopt SO_RCVTIMEO error: %s\n", strerror(errno));
        goto err;
    }

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        printf("connect socket error: %s\n", strerror(errno));
        goto err;
    }

    return fd;

err:
    if (fd > 0) close(fd);
    return -1;
}

/*****************************************************************************
    函 数 名 : ss_send
    功能描述 : 发送数据
    输入参数 : int fd
               int type
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月3日
*****************************************************************************/
static int ss_send(int fd, int type)
{
    int len;
    ss_msg *msg;
    char buf[1024] = {0};

    msg = (ss_msg *)buf;
    msg->type = type;
    len = send(fd, buf, sizeof(ss_msg), 0);
    if (len <= 0) {
        printf("send data error: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return 0;
}

/*****************************************************************************
    函 数 名 : ss_recv
    功能描述 : 接收数据
    输入参数 : int fd
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月3日
*****************************************************************************/
static void ss_recv(int fd)
{
    int len;
    ss_msg *msg;
    char buf[1024] = {0};

    len = recv(fd, buf, sizeof(buf), 0);
    if (len > 0) {
        msg = (ss_msg *)buf;
        printf("Resp: type = %d, result = %d\n", msg->type, msg->result);
    } else if (len < 0 && errno == EAGAIN) {
        printf("Resp: recv timeout\n");
    }

    close(fd);
}

/*****************************************************************************
    函 数 名 : main
    功能描述 : 主函数
    输入参数 : int argc
               char *argv[]
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月3日
*****************************************************************************/
int main(int argc, char *argv[])
{
    int ch;
    int type = -1;

    while ((ch = getopt(argc, argv, "h:t:")) != -1) {
        switch(ch) {
        case 't':
            type = ss_check_type(atoi(optarg));
            break;
        case 'h':
        default:
            ss_usage();
            return -1;
        }
    }

    if (type < 0) {
        ss_usage();
        return -1;
    }

    int fd = ss_socket();
    SS_RETURN_RES(fd < 0, -1);

    int ret = ss_send(fd, type);
    SS_RETURN_RES(ret < 0, -1);
    ss_recv(fd);

    return 0;
}

