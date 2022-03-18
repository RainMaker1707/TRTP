/***
 * A set of logging macro and functions that can be used.
 */

#ifndef __LOG_H_
#define __LOG_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _COLOR
/* Want more/other colors? See https://stackoverflow.com/a/3219471 and
 * https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
 */
#define ANSI_COLOR_BRIGHT_RED "\x1b[91m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#else
#define ANSI_COLOR_BRIGHT_RED
#define ANSI_COLOR_CYAN
#define ANSI_COLOR_RESET
#endif

#define _LOG(color, prefix, msg, ...)\
    do {\
        fprintf(stderr, color prefix msg ANSI_COLOR_RESET "\n", ##__VA_ARGS__);\
    } while (0)

#define ERROR(msg, ...) _LOG(ANSI_COLOR_BRIGHT_RED, "[ERROR] ", msg, ##__VA_ARGS__)

#ifdef _DEBUG
#define DEBUG(msg, ...) _LOG(ANSI_COLOR_CYAN, "[DEBUG] ", msg, ##__VA_ARGS__)
#else
#define DEBUG(msg, ...)
#endif

/* Displays an error if `cond` is not true */
/* Maybe it could also stop the program ? */
#define ASSERT(cond) if (!(cond)) { ERROR("Assertion \"%s\" failed at %s:%d", #cond, __FILE__, __LINE__); }

/* Prints `len` bytes starting from `bytes` to stderr */
void dump(const uint8_t *bytes, size_t len);

/* Use this useful macro instead of the bare function*/
#ifdef _DEBUG
#define DEBUG_DUMP(bytes, len) \
    do {                       \
        DEBUG("Dumping %ld bytes from pointer %p at %s:%d", (size_t) len, bytes, __FILE__, __LINE__); \
        dump((const uint8_t *) bytes, (size_t) len); \
    } while (0)
#else
#define DEBUG_DUMP(bytes, len)
#endif

#endif // __LOG_H_