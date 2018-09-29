/******************************************************************************

            版权所有 (C), 2017-2018, xxx Co.xxx, Ltd.

 ******************************************************************************
    文 件 名 : ss_epoll.c
    版 本 号 : V1.0
    作    者 : zc
    生成日期 : 2018年8月2日
    功能描述 : epoll封装接口
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

#include "ss_epoll.h"

/* 事件注册函数 */
static ss_event_loop event_loop;

/* 读、写数据回调函数 */
static void ss_read_from_client(struct ss_event_loop_s *eventLoop,
    int fd, int mask, void *privdata);
static void ss_write_to_client(struct ss_event_loop_s *eventLoop,
    int fd, int mask, void *privdata);

#define ss_event_loop_el(loop) (loop) = &event_loop

/*****************************************************************************
    函 数 名 : ss_add_event
    功能描述 : 添加事件
    输入参数 : ss_event_loop *eventLoop
               int fd
               int mask
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_add_event(ss_event_loop *eventLoop, int fd, int mask)
{
    int op;
    struct epoll_event ee = {0};

    op = eventLoop->events[fd].mask == SS_NONE ?
        EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    ee.events = 0;
    mask |= eventLoop->events[fd].mask;
    if (mask & SS_READABLE) ee.events |= EPOLLIN;
    if (mask & SS_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (epoll_ctl(eventLoop->epfd, op, fd, &ee) < 0) {
        printf("epoll_ctl error: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*****************************************************************************
    函 数 名 : ss_del_event
    功能描述 : 删除事件
    输入参数 : ss_event_loop *eventLoop
               int fd
               int mask
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_del_event(ss_event_loop *eventLoop, int fd, int mask)
{
    SS_RETURN(fd >= eventLoop->setsize);
    ss_file_event *fe = &eventLoop->events[fd];
    SS_RETURN(fe->mask == SS_NONE);

    struct epoll_event ee = {0};
    int delmask = eventLoop->events[fd].mask & (~mask);

    ee.events = 0;
    if (delmask & SS_READABLE) ee.events |= EPOLLIN;
    if (delmask & SS_WRITABLE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (delmask != SS_NONE) {
        epoll_ctl(eventLoop->epfd, EPOLL_CTL_MOD, fd, &ee);
    } else {
        /* 
         * Note, Kernel < 2.6.9 requires a non null event pointer
         * even for EPOLL_CTL_DEL.
         */
        epoll_ctl(eventLoop->epfd, EPOLL_CTL_DEL, fd, &ee);
    }

    fe->mask = fe->mask & (~mask);
    if (fd == eventLoop->maxfd && fe->mask == SS_NONE) {
        /* Update the max fd */
        int j;

        for (j = eventLoop->maxfd-1; j >= 0; j--) {
            if (eventLoop->events[j].mask != SS_NONE) {
                break;
            }
        }
        eventLoop->maxfd = j;
    }
}

/*****************************************************************************
    函 数 名 : ss_create_event
    功能描述 : 创建事件
    输入参数 : ss_event_loop *eventLoop
               int fd
               int mask 
               ss_event_proc proc
               void *clientData
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_create_event(ss_event_loop *eventLoop, int fd, int mask,
    ss_event_proc proc, void *clientData)
{
    SS_RETURN_RES(fd >= eventLoop->setsize, -1);

    ss_file_event *fe = &eventLoop->events[fd];
    SS_RETURN_RES(ss_add_event(eventLoop, fd, mask) < 0, -1);

    fe->mask |= mask;
    if (mask & SS_READABLE) fe->reventProc = proc;
    if (mask & SS_WRITABLE) fe->weventProc = proc;
    fe->clientdata = clientData;
    if (fd > eventLoop->maxfd) {
        eventLoop->maxfd = fd;
    }

    return 0;
}

/*****************************************************************************
    函 数 名 : ss_close_client
    功能描述 : 关闭客户端连接
    输入参数 : ss_event_loop *eventLoop
               ss_client *c
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_close_client(ss_event_loop *eventLoop, ss_client *c)
{
    if (c->fd > 0) {
        ss_del_event(eventLoop, c->fd, SS_READABLE);
        ss_del_event(eventLoop, c->fd, SS_WRITABLE);
        close(c->fd);
        c->fd = -1;
    }
}

/*****************************************************************************
    函 数 名 : ss_set_reuse_addr
    功能描述 : 设置地址重用
    输入参数 : int fd
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_set_reuse_addr(int fd)
{
    int opt = 1;

    /* 
     * Make sure connection-intensive things like the redis benckmark
     * will be able to close/open sockets a zillion of times
     */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        printf("setsockopt SO_REUSEADDR error: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*****************************************************************************
    函 数 名 : ss_create_socket
    功能描述 : 创建socket
    输入参数 : void
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_create_socket(void)
{
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        printf("create socket error: %s\n", strerror(errno));
        return -1;
    }

    /* 
     * Make sure connection-intensive things like the redis benchmark
     * will be able to close/open sockets a zillion of times
     */
    if (ss_set_reuse_addr(fd) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

/*****************************************************************************
    函 数 名 : ss_listen
    功能描述 : 监听socket
    输入参数 : int fd
               struct sockaddr *sa
               socklen_t len
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_listen(int fd, struct sockaddr *sa, socklen_t len)
{
    if (bind(fd, sa, len) < 0) {
        printf("bind socket error: %s\n", strerror(errno));
        return -1;
    }

    if (listen(fd, 512) < 0) {
        printf("listen socket error: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*****************************************************************************
    函 数 名 : ss_set_block
    功能描述 : 设置socket阻塞模式
    输入参数 : int fd
               int non_block
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_set_block(int fd, int non_block)
{
    int flags;

    /* 
     * Set the socket blocking (if non_block is zero) or non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal.
     */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        printf("fcntl(F_GETFL): %s\n", strerror(errno));
        return -1;
    }

    if (non_block) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, flags) == -1) {
        printf("fcntl(F_SETFL, O_NONBLOCK): %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*****************************************************************************
    函 数 名 : ss_read_from_client
    功能描述 : 读取客户端数据
    输入参数 : struct ss_event_loop_s *eventLoop
               int fd
               int mask
               void *privdata
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_read_from_client(struct ss_event_loop_s *eventLoop,
    int fd, int mask, void *privdata)
{
    int len;
    int ret = -1;
    ss_client *c = (ss_client *)privdata;

    memset(c->rbuf, 0, sizeof(c->rbuf));
    memset(c->wbuf, 0, sizeof(c->wbuf));

    len = recv(fd, c->rbuf, sizeof(c->rbuf), 0);
    if (len < 0) {
        if (errno == EAGAIN) {
            return;
        } else {
            printf("reading from client error: %s\n", strerror(errno));
            ss_close_client(eventLoop, c);
            return;
        }
    } else if (len == 0) {
        ss_close_client(eventLoop, c);
        return;
    }

    ss_msg *req = (ss_msg *)c->rbuf;
    ss_msg *rsp = (ss_msg *)c->wbuf;
    ss_msg_deal(req, rsp);
    c->wlen = sizeof(ss_msg) + rsp->len;

    ss_del_event(eventLoop, c->fd, SS_READABLE);
    ss_create_event(eventLoop, c->fd, SS_WRITABLE, ss_write_to_client, c);
}

/*****************************************************************************
    函 数 名 : ss_write_to_client
    功能描述 : 向客户端发送数据
    输入参数 : struct ss_event_loop_s *eventLoop
               int fd
               int mask
               void *privdata
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_write_to_client(struct ss_event_loop_s *eventLoop,
    int fd, int mask, void *privdata)
{
    int len;
    ss_client *c = (ss_client *)privdata;

    len = send(fd, c->wbuf, c->wlen, 0);
    if (len < 0) {
        if (errno == EAGAIN) {
            return;
        } else {
            printf("writing to client error: %s\n", strerror(errno));
            ss_close_client(eventLoop, c);
        }
    } else if (len == 0) {
        ss_close_client(eventLoop, c);
        return;
    }

    ss_del_event(eventLoop, c->fd, SS_WRITABLE);
    ss_create_event(eventLoop, c->fd, SS_READABLE, ss_read_from_client, c);
}

/*****************************************************************************
    函 数 名 : ss_accept
    功能描述 : 等待客户端连接
    输入参数 : int s
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_accept(int s)
{
    int fd;
    struct sockaddr_un sa;
    socklen_t salen = sizeof(sa);

    while (1) {
        fd = accept(s, (struct sockaddr *)&sa, &salen);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                printf("accept error: %s\n", strerror(errno));
                return -1;
            }
        }
        break;
    }

    return fd;
}

/*****************************************************************************
    函 数 名 : ss_accept_handler
    功能描述 : accept回调函数
    输入参数 : struct ss_event_loop_s *eventLoop
               int fd
               int mask
               void *privdata
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_accept_handler(struct ss_event_loop_s *eventLoop,
    int fd, int mask, void *privdata)
{
    int i;
    int ret;
    int cfd = -1;
    ss_client *c;

    SS_UNUSED(mask);
    SS_UNUSED(privdata);

    cfd = ss_accept(fd);
    SS_GOTO_LABLE(cfd < 0, err);
    SS_GOTO_LABLE(cfd >= eventLoop->setsize, err);

    c = &eventLoop->client[cfd];
    ss_set_block(cfd, 1);
    ret = ss_create_event(eventLoop, cfd, SS_READABLE,
        ss_read_from_client, c);
    SS_GOTO_LABLE(ret < 0, err);
    memset(c->rbuf, 0, sizeof(c->rbuf));
    memset(c->wbuf, 0, sizeof(c->wbuf));
    c->fd = cfd;

    return;

err:
    if (cfd >= 0) close(cfd);
    return;
}

/*****************************************************************************
    函 数 名 : ss_init_event_loop
    功能描述 : 初始化事件
    输入参数 : ss_event_loop *eventLoop
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static void ss_init_event_loop(ss_event_loop *eventLoop)
{
    int i;

    eventLoop->maxfd = -1;
    eventLoop->setsize = SS_EVENT_MAX;
    for (i = 0; i < eventLoop->setsize; i++) {
        eventLoop->events[i].mask = SS_NONE;
    }
}

/*****************************************************************************
    函 数 名 : ss_event_wait
    功能描述 : 等待事件响应
    输入参数 : ss_event_loop *eventLoop
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
static int ss_event_wait(ss_event_loop *eventLoop)
{
    int retval;
    int numevents = 0;

    retval = epoll_wait(eventLoop->epfd,
                        eventLoop->ep_events,
                        eventLoop->setsize, 1000);
    if (retval > 0) {
        int i;
        numevents = retval;

        for (i = 0; i < numevents; i++) {
            int mask = 0;
            struct epoll_event *e = &eventLoop->ep_events[i];

            if (e->events & EPOLLIN) mask |= SS_READABLE;
            if (e->events & EPOLLOUT) mask |= SS_WRITABLE;
            if (e->events & EPOLLERR) mask |= SS_WRITABLE;
            if (e->events & EPOLLHUP) mask |= SS_WRITABLE;
            eventLoop->fired[i].fd = e->data.fd;
            eventLoop->fired[i].mask = mask;
        }
    }

    return numevents;
}

/*****************************************************************************
    函 数 名 : ss_process_events
    功能描述 : 处理事件
    输入参数 : 无
    输出参数 : 无
    返 回 值 : 无
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
void ss_process_events(void)
{
    int i;
    int numevents;
    ss_event_loop *eventLoop;

    ss_event_loop_el(eventLoop);
    numevents = ss_event_wait(eventLoop);

    for (i = 0; i < numevents; i++) {
        ss_file_event *fe = &eventLoop->events[eventLoop->fired[i].fd];
        int mask = eventLoop->fired[i].mask;
        int fd = eventLoop->fired[i].fd;
        int rfired = 0;

        /* 调用read回调函数 */
        if (fe->mask & mask & SS_READABLE) {
            rfired = 1;
            fe->reventProc(eventLoop, fd, mask, fe->clientdata);
        }
        /* 调用write回调函数 */
        if (fe->mask & mask & SS_WRITABLE) {
            if (!rfired || (fe->weventProc != fe->reventProc)) {
                fe->weventProc(eventLoop, fd, mask, fe->clientdata);
            }
        }
    }
}

/*****************************************************************************
    函 数 名 : ss_init_server
    功能描述 : 初始化服务
    输入参数 : 无
    输出参数 : 无
    返 回 值 : int
    作    者 : zc
    日    期 : 2018年8月2日
*****************************************************************************/
int ss_init_server(void)
{
    int ret;
    int fd = -1;
    int epoll_fd = -1;
    struct sockaddr_un sa;
    ss_event_loop *eventLoop;

    ss_event_loop_el(eventLoop);
    ss_init_event_loop(eventLoop);

    epoll_fd = epoll_create(512);
    SS_GOTO_LABLE(epoll_fd < 0, err);

    fd = ss_create_socket();
    SS_GOTO_LABLE(fd < 0, err);

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    snprintf(sa.sun_path, sizeof(sa.sun_path), SS_UNIX_SOCKET);
    unlink(sa.sun_path);
    ret = ss_listen(fd, (struct sockaddr *)&sa, sizeof(sa));
    SS_GOTO_LABLE(ret < 0, err);

    ss_set_block(fd, 1);
    eventLoop->sfd = fd;
    eventLoop->epfd = epoll_fd;
    ret = ss_create_event(eventLoop, fd, SS_READABLE, ss_accept_handler, NULL);
    SS_GOTO_LABLE(ret < 0, err);

    return 0;

err:
    if (fd >= 0) close(fd);
    return -1;
}

