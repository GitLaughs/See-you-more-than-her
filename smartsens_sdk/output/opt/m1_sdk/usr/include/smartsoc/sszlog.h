#ifndef __SSZLOG_H__
#define __SSZLOG_H__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "zlog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef zlog_category_t*  log_rule_t;

log_rule_t  sdk_log_get_rule(char *pRule);

#define sdk_log_debug(cat, format, args...)     zlog_debug(cat, format, ##args)
#define sdk_log_info(cat, format, args...)      zlog_info(cat, format, ##args)
#define sdk_log_warn(cat, format, args...)      zlog_warn(cat, format, ##args)
#define sdk_log_error(cat, format, args...)     zlog_error(cat, format, ##args)
#define sdk_log_notice(cat, format, args...)    zlog_notice(cat, format, ##args)
#define sdk_log_fatal(cat, format, args...)     zlog_fatal(cat, format, ##args)


#ifdef __cplusplus
}
#endif

#endif // __SSZLOG_H__

