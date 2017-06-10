#ifndef LOGGING_H
#define LOGGING_H

#define LVL_DEBUG 5
#define LVL_INFO 4
#define LVL_WARNING 3
#define LVL_ERROR 2
#define LVL_CRITICAL 1

void logdebug(const char *msg);
void loginfo(const char *msg);
void logwarning(const char *msg);
void logerror(const char *msg);
void logcritical(const char *msg);

void flogdebug(const char *fmt, ...);
void floginfo(const char *fmt, ...);
void flogwarning(const char *fmt, ...);
void flogerror(const char *fmt, ...);
void flogcritical(const char *fmt, ...);

#endif /* LOGGING_H */
