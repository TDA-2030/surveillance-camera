
#include <stdio.h>
#include <string.h>
#include "rtsp_utility.h"

static const char *TAG = "rtsp_utility";

static const char *RTSP_VERSION = "RTSP/1.0";
static const char *USER_AGENT = "ESP32 (IDF:v4.4-dev-1594-g1d7068e4be-dirty)";


const char *rtsp_get_version(void)
{
    return RTSP_VERSION;
}

const char *rtsp_get_user_agent(void)
{
    return USER_AGENT;
}

typedef struct {
    uint32_t code;
    const char *describe;
} status_code_t;

/**
 * https://www.rfc-editor.org/rfc/rfc2326.html
 */
static const status_code_t g_status_code[] = {
    {100, "100 Continue"},
    {200, "200 OK"},
    {201, "201 Created"},
    {250, "250 Low on Storage Space"},
    {300, "300 Multiple Choices"},
    {301, "301 Moved Permanently"},
    {302, "302 Moved Temporarily"},
    {303, "303 See Other"},
    {304, "304 Not Modified"},
    {305, "305 Use Proxy"},
    {400, "400 Bad Request"},
    {401, "401 Unauthorized"},
    {402, "402 Payment Required"},
    {403, "403 Forbidden"},
    {404, "404 Not Found"},
    {405, "405 Method Not Allowed"},
    {406, "406 Not Acceptable"},
    {407, "407 Proxy Authentication Required"},
    {408, "408 Request Time-out"},
    {410, "410 Gone"},
    {411, "411 Length Required"},
    {412, "412 Precondition Failed"},
    {413, "413 Request Entity Too Large"},
    {414, "414 Request-URI Too Large"},
    {415, "415 Unsupported Media Type"},
    {451, "451 Parameter Not Understood"},
    {452, "452 Conference Not Found"},
    {453, "453 Not Enough Bandwidth"},
    {454, "454 Session Not Found"},
    {455, "455 Method Not Valid in This State"},
    {456, "456 Header Field Not Valid for Resource"},
    {457, "457 Invalid Range"},
    {458, "458 Parameter Is Read-Only"},
    {459, "459 Aggregate operation not allowed"},
    {460, "460 Only aggregate operation allowed"},
    {461, "461 Unsupported transport"},
    {462, "462 Destination unreachable"},
    {500, "500 Internal Server Error"},
    {501, "501 Not Implemented"},
    {502, "502 Bad Gateway"},
    {503, "503 Service Unavailable"},
    {504, "504 Gateway Time-out"},
    {505, "505 RTSP Version not supported"},
    {551, "551 Option not supported"},
};

const char *rtsp_get_status_from_code(uint32_t code)
{
    uint16_t num = sizeof(g_status_code) / sizeof(status_code_t);
    for (size_t i = 0; i < num; i++) {
        if (code == g_status_code[i].code) {
            return g_status_code[i].describe;
        }
    }
    return "500 Internal Server Error";
}

/**
 Define rtsp method
 method            direction        object     requirement
 DESCRIBE          C->S             P,S        recommended
 ANNOUNCE          C->S, S->C       P,S        optional
 GET_PARAMETER     C->S, S->C       P,S        optional
 OPTIONS           C->S, S->C       P,S        required
                                             (S->C: optional)
 PAUSE             C->S             P,S        recommended
 PLAY              C->S             P,S        required
 RECORD            C->S             P,S        optional
 REDIRECT          S->C             P,S        optional
 SETUP             C->S             S          required
 SET_PARAMETER     C->S, S->C       P,S        optional
 TEARDOWN          C->S             P,S        required
 */
const rtsp_methods_t rtsp_methods[RTSP_UNKNOWN] = {
    {RTSP_OPTIONS, "OPTIONS"},
    {RTSP_DESCRIBE, "DESCRIBE"},
    {RTSP_SETUP, "SETUP"},
    {RTSP_PLAY, "PLAY"},
    {RTSP_RECORD, "RECORD"},
    {RTSP_PAUSE, "PAUSE"},
    {RTSP_ANNOUNCE, "ANNOUNCE"},
    {RTSP_TEARDOWN, "TEARDOWN"},
    {RTSP_GET_PARAMETER, "GET_PARAMETER"},
    {RTSP_SET_PARAMETER, "SET_PARAMETER"},
};


char *rtsp_find_first_crlf(const char *str)
{
    while (*str) {
        if (str[0] == '\r' && str[1] == '\n') {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}

