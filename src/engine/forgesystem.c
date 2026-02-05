#include "forgesystem.h"
#include <stdarg.h>
#include <stdio.h>

void dbg_msg(const char *sys, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s] ", sys);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}
