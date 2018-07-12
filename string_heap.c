#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "error.h"

#define SITH_COLOR_RED     "\x1b[31m"
#define SITH_COLOR_GREEN   "\x1b[32m"
#define SITH_COLOR_BLUE    "\x1b[34m"
#define SITH_COLOR_CYAN    "\x1b[36m"
#define SITH_COLOR_RESET   "\x1b[0m"

#include "string_heap.h"

struct sith_heap_string {
    // Length of the enclosed string
    size_t length;
    // Memory allocated to this buffer, not counting the terminator
    size_t capacity;
    // Char buffer holding the delegate
    char* str_point;
};

HeapString* CreateHeapString(const char* content) {
    HeapString* string = calloc(1, sizeof (HeapString));
    if (content == NULL || string == NULL) return NULL;

    // Allocate enough space for string
    int size = strlen(content);
    char* str = calloc(size + 1, sizeof (char));
    if (str == NULL) {
        free(string);
        return NULL;
    }

    // Place the string and set all other fields
    memcpy(str, content, size);
    string->str_point = str;
    string->length = size;
    string->capacity = size;

    return string;
}

HeapString* CreateHeapStringL(char* content, unsigned long size) {
    HeapString* string = calloc(1, sizeof (HeapString));
    if (content == NULL || string == NULL) return NULL;
    if(size == 0) return CreateHeapString("");
    // Allocate the requested space
    char* str = calloc(size + 1, sizeof (char));
    if (str == NULL) {
        free(string);
        return NULL;
    }
    size_t len = strlen(content);
    if(len > size) len = size;
    // Place the string's chars up to "size", and set all other fields
    strncpy(str, content, size);
    string->str_point = str;
    string->length = len;
    string->capacity = size;
    return string;
}

size_t HeapStringLength(HeapString* this) {
    return this->length;
}

int HeapStringSet(HeapString* this, const char* content) {
    if (this == NULL || content == NULL) return SITH_RET_ERR;

    size_t len = strlen(content);
    if (this->capacity >= len + 1) {
        // New contents fit in, just copy chars and adjust length
        memcpy(this->str_point, content, len + 1);
        this->length = len;
    }
    else {
        // New contents are longer, reallocate
        char* new_point = realloc(this->str_point, len + 1);
        if (new_point == NULL) return SITH_RET_ERR;
        memcpy(new_point, content, len + 1);

        // Update all fields
        this->str_point = new_point;
        this->length = len;
        this->capacity = len;
    }
    return SITH_RET_OK;
}
int HeapStringSetSafe(HeapString* this, const HeapString* content) {
    if (this == NULL || content == NULL) return SITH_RET_ERR;

    size_t len = content->length;
    if (this->capacity >= len + 1) {
        // New contents fit in, just copy chars and adjust length
        memcpy(this->str_point, content->str_point, len + 1);
        this->length = len;
    }
    else {
        // New contents are longer, reallocate
        char* new_point = realloc(this->str_point, len + 1);
        if (new_point == NULL) return SITH_RET_ERR;
        memcpy(new_point, content->str_point, len + 1);

        // Update all fields
        this->str_point = new_point;
        this->length = len;
        this->capacity = len;
    }
    return SITH_RET_OK;
}


size_t HeapStringReplaceCharsAt(HeapString* this, char c, size_t pos, size_t num) {
    if (num == 0 || pos >= this->length) return 0;

    // Doing the old fashioned way to avoid any compiler inconsistencies
    size_t i = 0;
    for (i = pos; i < pos + num && i < this->length; i++) {
        this->str_point[i] = c;
    }

    return i - pos;
}

int HeapStringRemoveStart(HeapString* this, size_t num) {
    if (this == NULL || num == 0 || num >= this->length) return SITH_RET_ERR;

    // Copy char by char, terminator included
    for (size_t i = 0; i < this->length - num + 1; i++) {
        this->str_point[i] = this->str_point[i + num];
    }
    this->length -= num;

    return SITH_RET_OK;
}

int HeapStringTruncate(HeapString* this, size_t num) {
    if (this == NULL || num == 0 || num >= this->length) return SITH_RET_ERR;

    // Easy as moving down the terminator
    this->length -= num;
    this->str_point[this->length] = '\0';

    return SITH_RET_OK;
}

int HeapStringAppend(HeapString* this, char* content) {
    if (this == NULL || content == NULL) return SITH_RET_ERR;

    size_t len = strlen(content);
    size_t totalLen = this->length + len;

    if (this->capacity >= totalLen) {
        // Total length does not exceed capacity, copy new content and adjust length
        // Reminder: capacity value does not count terminator, but effective allocated memory does
        memcpy(this->str_point + this->length, content, len + 1);
        this->length += len;
    }
    else {
        // Exceeding capacity, reallocate
        char* new_point = realloc(this->str_point, totalLen + 1);
        if (new_point == NULL) return SITH_RET_ERR;
        memcpy(new_point + this->length, content, len + 1);

        // Update the structure
        this->str_point = new_point;
        this->length = totalLen;
        this->capacity = totalLen;
    }
    return SITH_RET_OK;
}

int HeapStringAppendSafe(HeapString* this, HeapString* string_to_append) {
    if (this == NULL || string_to_append == NULL) return SITH_RET_ERR;

    size_t len = string_to_append->length;
    size_t totalLen = this->length + len;

    if (this->capacity >= totalLen) {
        // Total length does not exceed capacity, copy new content and adjust length
        // Reminder: capacity value does not count terminator, but effective allocated memory does
        memcpy(this->str_point + this->length, string_to_append->str_point, len + 1);
        this->length += len;
    }
    else {
        // Exceeding capacity, reallocate
        char* new_point = realloc(this->str_point, totalLen + 1);
        if (new_point == NULL) return SITH_RET_ERR;
        memcpy(new_point + this->length, string_to_append->str_point, len + 1);

        // Update the structure
        this->str_point = new_point;
        this->length = totalLen;
        this->capacity = totalLen;
    }
    return SITH_RET_OK;
}


int HeapStringPrepend(HeapString* string, char* content) {
    if (string == NULL || content == NULL) return SITH_RET_ERR;

    size_t len = strlen(content);
    if (string->capacity >= string->length + len + 1) {
        memmove(string->str_point + len, string->str_point, string->length); //memmove explictly allows overlaps.
        strncpy(string->str_point, content, len);

        size_t old_len = string->length;
        *(string->str_point + len + old_len) = '\0';
        string->length = old_len + len;
    }
    else {
        char* new_str = calloc(string->length + len + 1, sizeof (char));
        if (new_str == NULL) return SITH_RET_ERR;
        strcpy(new_str, content);
        strcat(new_str, string->str_point);
        size_t old_len = string->length;
        free(string->str_point);

        string->str_point = new_str;
        string->length = old_len + len;
        string->capacity = old_len + len;
    }
    return SITH_RET_OK;
}


int GetLongFromHeapString(HeapString* s, long* res) {
    //char tal[MAX_STRING_CAPACITY] = {0};
    // char* tail = tal;
    long value = strtol(s->str_point, NULL, 10);
    if (/* *tail != '\0' || */ errno == ERANGE || value > LONG_MAX || value == 0) {
        return SITH_RET_ERR;
    }
    *res = value;
    return SITH_RET_OK;

}

char HeapStringCharAt(HeapString* string, size_t at){
    if(string == NULL) return '\0';
    if(at >= string->length) return '\0';
    return string->str_point[at];
}

int HeapStringSplitAtChar(HeapString* string, char subs, LinkedList* out){
    if(string == NULL || out == NULL) return SITH_RET_ERR;
    int occurrences = 0;
    int pos_old_sep = 0;
    for(size_t i = 0; i < string->length ; i++){
        if(string->str_point[i] == subs){
            HeapString* split = CreateHeapStringL(string->str_point + pos_old_sep, i - pos_old_sep);
            if(split == NULL) return SITH_RET_ERR;
            pos_old_sep = i +1;
            occurrences++;
            AppendToList(out, split);
        }
    }
    HeapString* split = CreateHeapString(string->str_point + pos_old_sep);
    if(split == NULL) return SITH_RET_ERR;
    occurrences++;
    AppendToList(out, split);
    return occurrences;
}

HeapString* HeapStringSplitAtCharFirst(HeapString* string, char subs){
    if(string == NULL) return NULL;
    for(size_t i = 0; i < string->length; i++){
        if(string->str_point[i] == subs){
            string->str_point[i] = '\0';
            HeapString* new_string = CreateHeapString(string->str_point);
            if(new_string == NULL) break;
            string->length = string->length - i - 1;
            memmove(string->str_point, string->str_point + i+ 1, string->length);
            string->str_point[string->length] = '\0';
            return new_string;
        }
    }
    return NULL;
}

HeapString* HeapStringSplitAtCharLast(HeapString* string, char subs){
    if(string == NULL) return NULL;
    unsigned short found_content = 0;
    for(size_t i = string->length; i > 0; i--){
        if(string->str_point[i - 1] != '\0' && string->str_point[i - 1] != subs
            && string->str_point[i -1] != '\n' && string->str_point[i -1] != '\r'){
             found_content = 1;
        }
        if(string->str_point[i - 1] == subs && found_content){
            return CreateHeapString(string->str_point + i);
        }
    }
    return NULL;
}

int HeapStringCheckSuffix(HeapString* string, char* suff){
    if(string == NULL){
        return SITH_RET_ERR;
    }
    if(suff == NULL) {
        return SITH_RET_ERR;
    }
    size_t len = strlen(suff);
    if(len > string->length) return SITH_RET_ERR;
    for(size_t i = len,j = string->length;  i > 0 && j > 0; i--,j--){
        if(string->str_point[j-1] != suff[i-1]) return SITH_RET_ERR;
    }
    return SITH_RET_OK;
}

int HeapStringRemoveEndTokens(HeapString* string){
    if(string == NULL) return SITH_RET_ERR;
    if(string->length <= 1) return SITH_RET_ERR;
    int token_removed = 0;
    if(string->str_point[string->length -1] == '\n' || string->str_point[string->length -1] == '\r'){
        string->str_point[string->length -1] = '\0';
        string->length--;
        token_removed = 1;
    }
    if(string->str_point[string->length -1] == '\r'){
        string->str_point[string->length -1] = '\0';
        string->length--;
        token_removed = 1;
    }
    return token_removed == 1 ? SITH_RET_OK : SITH_RET_ERR;
}

int HeapStringTrim(HeapString* string){
    if(string == NULL) return SITH_RET_ERR;
    size_t trails = 0;
    for(size_t i = 0; i<string->length; i++){
        if(string->str_point[i] == ' '){
            trails++;
        }
        else{
            break;
        }
    }
    if(trails > 0){
        memmove(string->str_point, string->str_point + trails, string->length - trails);
        string->str_point[string->length - trails] = '\0';
    }
    size_t old_trails = trails;
    for(size_t i = string->length - old_trails; i > 0; i--){
        if(string->str_point[i -1] == ' '){
            trails++;
            string->str_point[i-1] = '\0';
        }
        else{
            break;
        }
    }
    string->length -= trails;
    return SITH_RET_OK;
}


HeapString* HeapStringClone(HeapString* string){
    if(string == NULL) return NULL;
    return CreateHeapString(string->str_point);
}

int GetUnsigendLongFromHeapString(HeapString* s, unsigned long* res){
    if(s == NULL) return SITH_RET_ERR;
    char* tail = NULL;
    unsigned long long value = strtoul(s->str_point, &tail, 10);
    if (errno != 0) return SITH_RET_ERR;
    if (*tail != '\0') {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    if (value > UINT_MAX) {
        errno = ERANGE;
        return SITH_RET_ERR;
    }
    if(res != NULL) *res = (unsigned long) value;
    return SITH_RET_OK;
}


int HeapStringEquals(HeapString* this, HeapString* other) {
    if (this == NULL || other == NULL) return SITH_RET_ERR;
    if (this->length != other->length) return SITH_RET_ERR;
    if (strcmp(this->str_point, other->str_point) == 0) return SITH_RET_OK;
    return SITH_RET_ERR;
}

int HeapStringCompareRaw(HeapString* this, char* other) {
    if (this == NULL || other == NULL) return SITH_RET_ERR;
    if (strcmp(this->str_point, other) == 0) return SITH_RET_OK;
    return SITH_RET_ERR;
}

const char* HeapStringGetRaw(HeapString* this) {
    if (this == NULL) return NULL;
    return (const char*) this->str_point;
}

char* HeapStringInner(HeapString* this){
    if (this == NULL) return NULL;
    char* inner = this->str_point;
    this->str_point = NULL;
    free(this);
    return inner;
}

void DisposeHeapString(HeapString* this) {
    if (this == NULL) return;
    this->length = 0;
    this->capacity = 0;
    free(this->str_point);
    this->str_point = NULL;
    free(this);
}

int test_heap_string() {
    HeapString* s = CreateHeapString("   Lorenzo ");
    if (strcmp(s->str_point, "   Lorenzo ") != 0 || s->length != 11) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not constructed properly\n");
        return SITH_RET_ERR;
    }
    HeapStringTrim(s);
    if (strcmp(s->str_point, "Lorenzo") != 0 || s->length != 7) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not trimmed properly\n");
        return SITH_RET_ERR;
    }    
    /*   HeapString prefix = HeapStringPrefix(&s, 3);
       if(strcmp(prefix.str_point, "Lor") != 0 || prefix.lenght != 3){
           printf("%s    %zu\n",prefix.str_point, prefix.lenght);
           printf("["COLOR_RED"FAILED" COLOR_RESET "] String prefix is not getted properly\n");
           return -1;
       }
       HeapString postfix = HeapStringPostfix(&s, 3);
       if(strcmp(postfix.point_str, "nzo") != 0 || postfix.lenght != 3){
           printf("%s    %zu\n",postfix.str_point, postfix.lenght);
           printf("["COLOR_RED"FAILED" COLOR_RESET "] String postfix is not getted properly\n");
           return -1;
       }
     */
    HeapStringAppend(s, "Mazda");
    if (strcmp(s->str_point, "LorenzoMazda") != 0 || s->length != 12) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not appended properly\n");
        return SITH_RET_ERR;
    }
    HeapStringPrepend(s, "Mazda");
    if (strcmp(s->str_point, "MazdaLorenzoMazda") != 0 || s->length != 17) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not pre-appended properly\n");
        return SITH_RET_ERR;
    }
    HeapStringAppend(s, "9000");
    if (strcmp(s->str_point, "MazdaLorenzoMazda9000") != 0 || s->length != 21) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not appended properly\n");
        return SITH_RET_ERR;
    }
    HeapStringSet(s, "9000");
    if (strcmp(s->str_point, "9000") != 0 || s->length != 4) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not setted properly\n");
        return SITH_RET_ERR;
    }
    HeapStringPrepend(s, "Mazda");
    if (strcmp(s->str_point, "Mazda9000") != 0 || s->length != 9) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not pre-appended properly\n");
        return SITH_RET_ERR;
    }
    HeapStringAppend(s, "Mazda");
    if (strcmp(s->str_point, "Mazda9000Mazda") != 0 || s->length != 14) {
        printf("%s    %zu\n", s->str_point, s->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not appended properly\n");
        return SITH_RET_ERR;
    }

    HeapString* oth = CreateHeapString("Mazda9000Mazda");
    int res = HeapStringEquals(s, oth);
    if (res == SITH_RET_ERR) {
        printf("%s != %s   %zu != %zu\n", s->str_point, oth->str_point, s->length, oth->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] Strings are not compared properly\n");
        return SITH_RET_ERR;
    }
    res = HeapStringCompareRaw(s, "Mazda9000Mazda");
    if (res == SITH_RET_ERR) {
        printf("%s != %s   %zu != %zu\n", s->str_point, oth->str_point, s->length, oth->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not raw compared properly\n");
        return SITH_RET_ERR;
    }
    size_t bytes = HeapStringReplaceCharsAt(s, '\0', s->length - 1, 1);
    if (bytes == 0) {
        printf("%s != %s   %zu != %zu\n", s->str_point, oth->str_point, s->length, oth->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not replaced properly\n");
        return SITH_RET_ERR;
    }
    res = HeapStringTruncate(oth, 3);
    if (res == SITH_RET_ERR || oth->length != 11 || strcmp(oth->str_point, "Mazda9000Ma") != 0) {
        printf("%s     %zu\n", oth->str_point, oth->length);
        printf("["SITH_COLOR_RED"FAILED" SITH_COLOR_RESET "] String is not raw compared properly\n");
        return SITH_RET_ERR;
    }
    DisposeHeapString(s);
    DisposeHeapString(oth);
    /*    long t = 0;
        GetLongFromString(&s, &t);
        unsigned long tt = 0;
        GetUnsigendLongFromString(&s, &tt);
        if(t != 9000){
            printf("["COLOR_RED"FAILED" COLOR_RESET "] Integer is not extracted properly\n");
            return -1;
        }
        if
        (tt != 9000){
            printf("["COLOR_RED"FAILED" COLOR_RESET "]  Unsigned Integer is not extracted p\n");
            return -1;
        } */
    printf("[" SITH_COLOR_GREEN "OK" SITH_COLOR_RESET "] Good HeapString test passed\n");
    return SITH_RET_OK;

}
