/**
 * File:   string_heap.h
 * Author: llde
 * Brief:  A dynamically resizable string
 * 
 * 
 *
 * Created on 29 September 2017, 00:20
 */

#ifndef SITH_STRING_HEAP_H
#define SITH_STRING_HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"

/*
 * A structure that represent an heap allocated string
 * It store the pointer to the heap allocated char array,
 * the string lenght and the string capacity.
 * No member of HeapString should be used directly. Use the provided function.
 */
typedef struct sith_heap_string HeapString;

/*
 * Function CreateString
 * This function take a pointer to the first element of a string an allocate it in the Heap and
 * store the lenght of the string and it's capacity  in the HeapString structure
 * @param content: Pointer to the first element of an array of chars. Must be NULL terminated
 * passing a non-NULL terminated string is Undefined Beheviour
 * @Return an HeapString instance.
 */
HeapString* CreateHeapString(const char* content);

/*
 * Function CreateStringL
 * This function take a pointer to the first element of a string an allocate it in the Heap and
 * store the lenght of the string and it's capacity  in the HeapStirng structure
 * @param content: Pointer to the first element of an array of chars. May not be NULL terminated
 * @param size: the size of content array. Must be minor or equal to the actual size.
 * passing a size bigger then actual array is Undefined Beheviour.
 * @return an HeapString instance.
 */
HeapString* CreateHeapStringL(char* content, unsigned long size);

/**
 * 
 * @param this
 * @return 
 */
size_t HeapStringLength(HeapString* this);

/*
 * Function StringSet
 * Replace the content string with the provided content.
 * If the lenght of the new  content is bigger then the string capacity
 * this function allocate. Otherwise this doesn't allocate at all.
 * @param string. This, must be initialized.
 * @param content: NULL terminated char array that contain the content to be written in the string
 * Passing a non NULL terminated char array is UB.
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */
int HeapStringSet(HeapString* string, const char* content);


/*
 * Function StringSetSafe
 * Replace the content string with the new string.
 * If the lenght of the new  content is bigger then the string capacity
 * this function allocate. Otherwise this doesn't allocate at all.
 * @param string. This, must be initialized.
 * @param content:  Other HeaspString
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */

int HeapStringSetSafe(HeapString* string, const HeapString* content);

/*
 * Function StringAppend
 * Append the content string after the current string content
 * If the lenght of the new  content lus the lenght of the old content is bigger then
 * the string capacity this function allocate. Otherwise this doesn't allocate at all.
 * @param string. This, must be initialized.
 * @param content: NULL terminated char array that contain the content to be written in the string
 * Passing a non NULL terminated char array is UB.
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */
int HeapStringAppend(HeapString* string, char* content);

/*
 * Function StringAppendSafe
 * Append the content string after the current string content
 * If the lenght of the new  content lus the lenght of the old content is bigger then
 * the string capacity this function allocate. Otherwise this doesn't allocate at all.
 * @param string. This, must be initialized.
 * @param string_to_append: The HeapString that is appended.
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */

int HeapStringAppendSafe(HeapString* string, HeapString* string_to_append);

/*
 * Function StringPrefix
 * Replace the content
 *
 * @param string. This, must be initialized.
 * @param content: NULL terminated char array that contain the content to be written in the string
 * PAssing a non NULL terminated char array is UB.
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */
int HeapStringPrefix(HeapString* string, char* content, unsigned long prefix);

/*
 * HeapStringReplaceCharsAt
 *
 * Write in this string the char c at least for num times, from the position pos
 * pos + num cannot exceed the string's length
 * @param this
 * @param c: a char the is copied into the string
 * @param pos: the starting position for the copy
 * @num: the number of times that the char should be copied.
 * @return the actual number  of chars written. 0 if error happened.
 */
size_t HeapStringReplaceCharsAt(HeapString* this, char c, size_t pos, size_t num);


/*
 * HeapStringCharAt
 *
 * Get the char at the speicifed position
 * @param this
 * @param pos: the starting position for the copy
 * @return the char in the specified position. Return '\0' if not present.
 */
char HeapStringCharAt(HeapString* string, size_t at);

/**
 * Truncates the given string 
 * 
 * @param this
 * @param num
 * @return 
 */
int HeapStringTruncate(HeapString* this, size_t num);

/**
 * 
 * @param this
 * @param num
 * @return 
 */
int HeapStringRemoveStart(HeapString* this, size_t num);

/*
 * Function StringPostfix
 * Replace the content string with the provided content.
 * @param string. This, must be initialized.
 * @param content: NULL terminated char array that contain the content to be written in the string
 * PAssing a non NULL terminated char array is UB.
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */
int HeapStringPostfix(HeapString* string, char* content, unsigned long postfix);


/*
 * Function StringAppendPre
 * Append the content string before the current string content
 * If the lenght of the new  content plus the lenght of the old content is bigger then
 * the string capacity this function allocate. Otherwise this doesn't allocate at all.
 * @param string. This, must be initialized.
 * @param content: NULL terminated char array that contain the content to be written in the string
 * Passing a non NULL terminated char array is UB.
 * @return RETURN_SUCCESS if completed, RETURN_FAILURE otherwise
 */
int HeapStringPrepend(HeapString* string, char* content);

/*
 * Function HeapStringSplitAtCharFirst
 * Split the HeapString in on the first occurence of the separator char
 * @param string. This, must be initialized.
 * @param subs: Separator
 * @return A new HeapString* with the start..sep  string, NULL otherwise
 * Side Effect, the original string is truncated  
 */
HeapString* HeapStringSplitAtCharFirst(HeapString* string, char subs);


/*
 * Function HeapStringSplitAtChar
 * Split the HeapString on all occurences of the separator char
 * @param string. This, must be initialized.
 * @param subs: Separator
 * @param out: a LinkedList of HeapString pointer+.
 * @return the number of the created HeapString, 
 *   0 if separator not present or at first/last pos, SITH_ERR if an error occurred,
 */
int HeapStringSplitAtChar(HeapString* string, char subs, LinkedList* out);

/*
 * Function HeapStringSplitAtCharLAst
 * Split the HeapString in on the last occurence of the separator char
 * @param string. This, must be initialized.
 * @param subs: Separator
 * @return A new HeapString* with the sep..end string, NULL otherwise
 */
HeapString* HeapStringSplitAtCharLast(HeapString* string, char subs);


/*
 * Function HeapStringCheckSuffix
 * Check for the occurence of suff inside the string
 * @param string. This, must be initialized.
 * @param suff: the suffix
 * @return SITH_OK if true SITH_ERR if false
 */
int HeapStringCheckSuffix(HeapString* string, char* suff);

/*
 * Function HeapStringRemoveEndTokens
 * Sanitize a string removing \r or \n  or \r\n at the end 
 * @param string. This, must be initialized.
 * @return SITH_OK if success SITH_ERR if tokens not found
 * E chi ha bisogno di strtok che manco è thread safe
 */
int HeapStringRemoveEndTokens(HeapString* string);

/*
 * Function HeapStringTrim
 * Trim a string removing  trailing  spaces
 * @param string. This, must be initialized.
 * 
 * @return SITH_OK if success SITH_ERR if tokens not found
 * E chi ha bisogno di strtok che manco è thread safe
 */
int HeapStringTrim(HeapString* string);

/*
 * Function HeapStringClone
 * Clone this HeapString
 * @param string. This, must be initialized.
 * @return A new HeapString* with the same content of the original one. Deep Copy
 */
HeapString* HeapStringClone(HeapString* string);
/*
 * Function StringEquals
 * Return 0 if the contents of the two strings are equal.
 * Capacaty is not take into consideration.
 * @param this. other strings-
 * @param other: other string
 * @return RETURN_SUCCESS if equals, RETURN_FAILURE otherwise
 */
int HeapStringEquals(HeapString* this, HeapString* other);

/*
 * Function CompareRaw
 * Return 0 if the contents of the string is equals to the content of the array.
 * Capacaty is not take into consideration.
 * @param this. other strings-
 * @param other: NULL terminated char array.
 * @return RETURN_SUCCESS if equals, RETURN_FAILURE otherwise
 */
int HeapStringCompareRaw(HeapString* this, char* other);

/*
 * Function HeapStringGetRaw
 * Get a cost pointer to the underlying string
 * @param this : HeapString
 * @return Heap Allocated const pointer to first object of the stirng.
 */
const char* HeapStringGetRaw(HeapString* this);

/*
 * Function HeapStringInner
 * Get a  pointer to the underlying string and free this.
 * this cannot be used anymore.
 * It's responsability of the caller free the returned buffer.
 * @param this : HeapString
 * @return  pointer to first object of the string.
 * 
 */
char* HeapStringInner(HeapString* this);

/**
 * 
 * @param s
 * @param res
 * @return 
 */
int GetLongFromHeapString(HeapString* s, long* res);

/**
 * Convert the HeapString to an unsigned long, and return SITH_OK if succeeed. 
 * @param s the HeapString 
 * @param res a pointer to an unsigned long to be written with the results. 
 *    Can be NULL, in which case the  function only test the possibility of convertion
 * @return SITH_OK if the conversion is possible SITH_ERR otherwise.
 */
int GetUnsigendLongFromHeapString(HeapString* s, unsigned long* res);

/** 
 * DisposeHeapString
 * Destroy this HeapString and free relative memory.
 * @param this
 * 
 */
void DisposeHeapString(HeapString* this);

/**
 * 
 * @return 
 */
int test_heap_string();


#ifdef __cplusplus
}
#endif

#endif /* SITH_STRING_HEAP_H */
