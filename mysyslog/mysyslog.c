#include "mysyslog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

// Уровни журналирования
const char *level_str(int level) {
    switch (level) {
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERR: return "ERROR";
        default: return "UNKNOWN";
    }
}

// Запись в файл
void mysyslog(int level, const char *format, ...) {
    FILE *fp = fopen("/var/log/myrpc.log", "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] ",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        level_str(level)
    );

    va_list args;
    va_start(args, format);
    vfprintf(fp, format, args);
    va_end(args);

    fprintf(fp, "\n");
    fclose(fp);
}
