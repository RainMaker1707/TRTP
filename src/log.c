/***
 * A set of logging macro and functions that can be used.
 */

#include "log.h"

/* Prints `len` bytes starting from `bytes` to stderr */
void dump(const uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len;) {
        fprintf(stderr, "%04x:  ", (int) i);

        for (size_t j = 0; j < 16 && i + j < len; j++) {
            fprintf(stderr, "%02x ", bytes[i + j]);
        }
        fprintf(stderr, "\t");
        for (size_t j = 0; j < 16 && i < len; j++, i++) {
            fprintf(stderr,"%c ", bytes[i]);
        }
        fprintf(stderr,"\n");
    }
}