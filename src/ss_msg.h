/******************************************************************************

            版权所有 (C), 2017-2018, xxx Co.xxx, Ltd.

 ******************************************************************************
    文 件 名 : ss_msg.h
    版 本 号 : V1.0
    作    者 : zc
    生成日期 : 2018年8月2日
    功能描述 : 消息结构头文件
    修改历史 :
******************************************************************************/
#ifndef _SS_MSG_H_
#define _SS_MSG_H_

#define SS_BUFER_MAX  1400

#define SS_UNUSED(x) (void)(x)
#define SS_RETURN(_cond) do { \
    if ((_cond)) return; } while (0)
#define SS_RETURN_RES(_cond, _res) do { \
    if ((_cond)) return (_res); } while (0)
#define SS_GOTO_LABLE(_cond, _lable) do { \
    if ((_cond)) goto _lable; } while (0)

/* 消息类型 */
enum SS_MSG_TYPE {
    SS_MSG_TYPE_MIN = 0,
#define SS_TEST_MSG        "Test message"
    SS_TEST_MSG_NUM,
#define SS_RESP_RESULT     "Response message"
    SS_RESP_RESULT_NUM,

    /* ...... */

    SS_MSG_TYPE_MAX,
};

/* 通过消息类型获取信息 */
#define SS_MSG_BY_TYPE_OF_INFO(_type) ({ \
    const char *_msg;                 \
    switch ((_type)) {                \
    case SS_TEST_MSG_NUM:    (_msg) = SS_TEST_MSG; break; \
    case SS_RESP_RESULT_NUM: (_msg) = SS_RESP_RESULT; break; \
    default: (_msg) = NULL; break; }  \
    _msg; })

/* 消息结构体 */
typedef struct ss_msg_s {
    int type;
    /* Result of msg processing */
    int result;
    /* Length of segment buffer. */
    int len;
    /* Address of segment buffer. */
    char data[];
} ss_msg;

/* 消息初始化 */
void ss_msg_init(void);

/* 消息处理 */
void ss_msg_deal(ss_msg *req, ss_msg *resp);

#endif

