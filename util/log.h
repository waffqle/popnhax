#ifndef __LOG_H__
#define __LOG_H__

#include <cstdio>

#include <cstddef>
#include <cstdint>

extern FILE *g_log_fp;

#define LOG(...) do { \
if (g_log_fp != NULL) { \
fprintf(g_log_fp, __VA_ARGS__); \
fflush(g_log_fp);\
 }\
fprintf(stderr, __VA_ARGS__); \
} while (0)

#endif