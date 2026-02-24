#ifndef _ERR_H
#define _ERR_H
#ifdef __GNUC__
#define err_noreturn __attribute__((noreturn))
//#define err_noreturn [[noreturn]]
#else
#define err_noreturn
#endif
err_noreturn void err(int x, const char* f, ...);
err_noreturn void errx(int x, const char* f, ...);
void warn(const char* f, ...);
void warnx(const char* f, ...);
#include <stdarg.h>
void verr(int x, const char* f, va_list a);
void verrx(int x, const char* f, va_list a);
void vwarn(const char* f, va_list a);
void vwarnx(const char* f, va_list a);
#endif
