#ifndef MYSYSLOG_H
#define MYSYSLOG_H

#define LOG_INFO 1
#define LOG_WARNING 2
#define LOG_ERR 3

void mysyslog(int level, const char *format, ...);

#define log_info(...)    mysyslog(LOG_INFO, __VA_ARGS__)
#define log_warning(...) mysyslog(LOG_WARNING, __VA_ARGS__)
#define log_error(...)   mysyslog(LOG_ERR, __VA_ARGS__)

#endif
