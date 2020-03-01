#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>

#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_DEF "\033[0m"

#ifdef DEBUG_APP
#define pr_debug(...) do { printf(COLOR_GREEN"DEBUG:" __VA_ARGS__); puts(COLOR_DEF""); } while(0)
#else
#define pr_debug(...)
#endif
#define pr_info(...) do { printf(__VA_ARGS__); puts(""); } while(0)
#define pr_err(...) do { printf(COLOR_RED "ERROR:"  __VA_ARGS__); puts(COLOR_DEF ""); } while(0)
#define pr_warn(...) do { printf( "WARNING:"  __VA_ARGS__); puts(""); } while(0)
#define pr_cont(...) do { printf(__VA_ARGS__); } while(0)

#endif /* DEFS_H */
