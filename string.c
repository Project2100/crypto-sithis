#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "string.h"
#include "plat.h"

int getInteger(char* s, int* t) {
    char* tail;

    int olderr = errno;
    errno = 0;


    long value = strtol(s, &tail, 10);
    if (errno != 0) return -1;
    if (*tail != '\0') {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    if (value > INT_MAX || value < INT_MIN) {
        errno = ERANGE;
        return SITH_RET_ERR;
    }

    *t = (int) value;
    errno = olderr;
    return SITH_RET_OK;
}

int getUInteger(char* s, unsigned int* t) {
    char* tail;

    int olderr = errno;
    errno = 0;

    unsigned long value = strtoul(s, &tail, 10);
    if (errno != 0) return -1;
    if (*tail != '\0') {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    if (value > UINT_MAX) {
        errno = ERANGE;
        return SITH_RET_ERR;
    }

    *t = (unsigned int) value;
    errno = olderr;
    return SITH_RET_OK;
}

int getUShort(char* s, unsigned short* t) {
    char* tail;

    int olderr = errno;
    errno = 0;


    unsigned long value = strtoul(s, &tail, 10);
    if (errno != 0) return -1;
    if (*tail != '\0') {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    if (value > USHRT_MAX) {
        errno = ERANGE;
        return SITH_RET_ERR;
    }

    *t = (unsigned short) value;
    errno = olderr;
    return SITH_RET_OK;
}

int getUByte(char* s, unsigned char* t) {
    char* tail;

    int olderr = errno;
    errno = 0;


    unsigned long value = strtoul(s, &tail, 10);
    if (errno != 0) return -1;
    if (*tail != '\0') {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    if (value > UCHAR_MAX) {
        errno = ERANGE;
        return SITH_RET_ERR;
    }

    *t = (unsigned char) value;
    errno = olderr;
    return SITH_RET_OK;
}

char* flatten(char*const* strings, size_t count, char separator) {
    int finalSize = 0;

    int* sizes = malloc(count * sizeof (int));
    if (sizes == NULL) {
        return NULL;
    }

    // Calculate flattened string length and keep single lengths
    for (size_t i = 0; i < count; i++) {
        finalSize += (sizes[i] = strlen(strings[i])) + 1;
    }

    char* output = malloc(finalSize * sizeof (char));
    if (output == NULL) {
        free(sizes);
        return NULL;
    }

    char* caret = output;

    // Do flatten
    for (size_t i = 0; i < count; i++) {
        if (sizes[i] == 0) continue;
        memcpy(caret, strings[i], sizes[i]);
        caret[sizes[i]] = ((i == count - 1) ? '\0' : separator);
        caret += sizes[i] + 1;
    }

    free(sizes);

    return output;
}

char** split(char* source, size_t length, char separator, size_t* count) {

    // Do correct length
    length = strnlen(source, length);

    *count = 0;
    int inToken = 0;

    // Count tokens
    // \0 is guaranteed not to appear
    for (size_t i = 0; i < length; i++) {
        if (inToken) {
            if (source[i] == separator) {
                inToken = 0;
            }
        }
        else {
            if (source[i] != separator) {
                inToken = 1;
                count++;
            }
        }
    }

    // Allocate tokens array
    char** tokens = calloc(*count, sizeof (char*));
    if (tokens == NULL) {
        return NULL;
    }

    // Extract tokens
    inToken = 0;
    int charCount = 0;
    size_t tokenIndex = 0;
    for (size_t i = 0; i < length; i++) {

        if (inToken) {
            if (source[i] == separator || i == length - 1) {
                inToken = 0;
                tokens[tokenIndex] = calloc(charCount + 1, sizeof (char));
                if (tokens[tokenIndex] == NULL) {
                    // No more memory, free all the tokens array and return
                    for (size_t j = 0; j < tokenIndex; j++) free(tokens[j]);
                    free(tokens);
                    return NULL;
                }
                memcpy(tokens[tokenIndex], source + i - charCount, charCount);
                tokenIndex++;
                charCount = 0;
            }
            else charCount++;
        }
        else {
            if (source[i] != separator) {
                inToken = 1;
                charCount++;
            }
        }
    }

    return tokens;
}

int replace(char* source, size_t length, char target, char repl) {
    int count = 0;
    for (size_t i = 0; i < length && source[i] != '\0'; i++)
        if (source[i] == target) {
            source[i] = repl;
            count++;
        }
    return count;
}

int compareUntilChar(char* s1, char* s2, size_t length, char delimiter) {
    if (length == 0 || s1 == NULL || s2 == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    for (size_t i = 0; i < length; i++) {
        if (s1[i] == '\0' || s2[i] == '\0' || s1[i] != s2[i]) {

            return 1;
        }

        // Strings aren't terminated and are equal so far
        if (s1[i] == delimiter) {
            // We're done
            return SITH_RET_OK;
        }
    }
    return SITH_RET_OK;
}
