/*
 * File:   string.h
 * Author: Project2100
 * Brief:  Utility functions and macros over char arrays for input conversion
 * 
 * 
 * Conversion ratio from decimal representation to binary: log2(10) ~ 3.341 ~ 93/28
 * Taken from: https://stackoverflow.com/questions/43787672/the-max-number-of-digits-in-an-int-based-on-number-of-bits
 * 
 * Created on 18 September 2017, 22:22
 */

#ifndef SITH_STRING_H
#define SITH_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

    // Digit counts for snsigned types
#define SITH_MAXCH_UINT (unsigned int) (CHAR_BIT * sizeof(int) * 28 / 93)
#define SITH_MAXCH_ULONG (unsigned int) (CHAR_BIT * sizeof(long) * 28 / 93)
#define SITH_MAXCH_ULONGLONG (unsigned int) (CHAR_BIT * sizeof(long long) * 28 / 93)

    // Digit counts for signed types
#define SITH_MAXCH_INT (unsigned int) ((CHAR_BIT * sizeof(int) - 1) * 28 / 93 + 1)
#define SITH_MAXCH_LONG (unsigned int) ((CHAR_BIT * sizeof(long) - 1) * 28 / 93 + 1)
#define SITH_MAXCH_LONGLONG (unsigned int) ((CHAR_BIT * sizeof(long long) - 1) * 28 / 93 + 1)


int getInteger(char* s, int* t);
int getUInteger(char* s, unsigned int* t);
int getUShort(char* s, unsigned short* t);
int getUByte(char* s, unsigned char* t);

/**
 * Concatenates the given array of strings into a newly allocated string,
 * using the given separator.</br>
 * Does not apply any quoting, and NULL tokens do not affect the result
 * 
 * @param strings
 * @param count
 * @param separator
 * @return 
 */
char* flatten(char*const* strings, size_t count, char separator);

/**
 * Returns an array of newly created tokens from the string 'source', separated
 * by 'separator'. Ignores quoting
 * 
 * @param source
 * @param len
 * @param separator
 * @param count
 * @return 
 */
char** split(char* source, size_t len, char separator, size_t* count);

/**
 * Replace all the occurrences of 'target' in 'source' to 'repl'
 * 
 * @param source
 * @param length
 * @param target
 * @param repl
 * @return 
 */
int replace(char* source, size_t length, char target, char repl);

/**
 * Campares s1, and s2 for equality up to the first occurrence of 'delimiter'.
 * If the delimiter is not found before reaching 'length', the function fails
 * 
 * @param s1
 * @param s2
 * @param length
 * @param delimiter
 * @return 
 */
int compareUntilChar(char* s1, char* s2, size_t length, char delimiter);


#ifdef __cplusplus
}
#endif

#endif /* SITH_STRING_H */
