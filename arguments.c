/*
 * File:   arguments.c
 * Author: llde
 *
 * Created on 20 January 2018, 22:22
 *
 * This want to be an easy of use framework for manage commandline option.
 * The priority of the configuration is: commandline win over configuration file
 * This is the second version  of the arguments and configuration file system system
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "string.h"
#include "arguments.h"
#include "string_heap.h"
#include "file.h"

#define STATE_ACCEPT_OPT_TOKEN 1
#define STATE_ACCEPT_FOWORD_TOKEN 2
#define STATE_END_GOOD 3
#define STATE_END_BAD 4

char* preamble = NULL;
Option* options = NULL;
size_t optionCount = 0;
char* config = NULL;
MaskPopulate masker = NULL;

void InitArguments(Option* opts, size_t number, char* pream, char* config_name, MaskPopulate populate_mask) {
    preamble = pream;
    optionCount = number;
    options = opts;
    masker = populate_mask;
    config = config_name;
    if (config_name == NULL) return;
    if (strcmp(config_name, SITH_OPT_EMPTY) == 0) return;
    if (CheckForConfigurationFile() == SITH_RET_ERR) {
        printf("Configuration file doesn't exist. Creating a new one with default parameters\n");
        fflush(stdout);
        IntializeDefaultConfigurationFile();
    }
    else {
        ReadConfigFile(NULL);
    }
    char* full_config = GetRealPath(config_name);
    if (full_config == NULL) {
        HandleErrorStatus("Failed to get full path. Will use the relative path. ");
    }
    else {
        config = full_config;
    }
    printf("Configuration file:  %s\n", config);
}

int CheckForConfigurationFile() {
    File* k = CreateFileObject(config, SITH_FS_INIT(0), SITH_FILEMODE_RO, SITH_OPENMODE_EXIST, 0);
    if (k == NULL) return SITH_RET_ERR; //doesn't exist.
    CloseFileObject(k);
    return SITH_RET_OK;
}

int IntializeDefaultConfigurationFile() {
    File* k = CreateFileObject(config, SITH_FS_INIT(0), SITH_FILEMODE_RW, SITH_OPENMODE_NEWONLY, 0);
    if (k == NULL) return SITH_RET_ERR; //exist.
    for (size_t i = 0; i < optionCount; i++) {
        if (strcmp(options[i].config_symbol, "") != 0) {
            size_t buf = strlen(options[i].config_symbol);
            //May be nice to handle multi-var args.
            printf("Processing %s  =  %s\n", options[i].config_symbol, options[i].value);
            int res = WriteToFileObject(k, options[i].config_symbol, &buf);
            buf = 1;
            res |= WriteToFileObject(k, "=", &buf);
            buf = strlen(options[i].value);
            res |= WriteToFileObject(k, options[i].value, &buf);
            buf = 1;
            res |= WriteToFileObject(k, "\n", &buf);
            if (res != 0) {
                HandleErrorStatus("Impossible to correctly write default configuration file");
                CloseFileObject(k);
                return SITH_RET_ERR;
            }
        }
    }
    CloseFileObject(k);
    return SITH_RET_OK;
}

int ParseArguments(int num, char** args) {
    if (options == NULL || optionCount == 0) {
        errno = ENOTSUP;
        return -1;
    }
    if (num < 0 || args == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    if (num == 1) return SITH_RET_OK;
    size_t state = STATE_ACCEPT_OPT_TOKEN;
    int currentIndex = 1;
    size_t index_in_options = -1;
    size_t number_param = 0;

    HeapString* appoggio = CreateHeapString("");
    if (appoggio == NULL) {
        // errno shall be already set as ENOMEM by CreateHeapString
        return SITH_RET_ERR;
    }

    while (state != STATE_END_GOOD && state != STATE_END_BAD) {
        char* currArg = args[currentIndex];
        if (state == STATE_ACCEPT_OPT_TOKEN) {
            if (currArg[0] != '-' || strlen(currArg) != 2) {
                printf("Parameter %s non valid\n", currArg);
                state = STATE_END_BAD;
            }
            else {
                int found = 0;
                for (size_t i = 0; i < optionCount; i++) {
                    if (options[i].symbol != currArg[1]) continue;
                    if (options[i].number_args == 0) {
                        strcpy(options[i].value, SITH_OPT_TRUE);
                        found = 1;
                        break;
                    }
                    number_param = options[i].number_args;
                    index_in_options = i;
                    state = STATE_ACCEPT_FOWORD_TOKEN;
                    found = 1;
                    break;
                }
                if (found == 0) {
                    printf("Parameter %s non valid\n", currArg);
                    state = STATE_END_BAD;
                }
            }
            currentIndex++;
        }
        else if (state == STATE_ACCEPT_FOWORD_TOKEN) {
            if (number_param != options[index_in_options].number_args) {
                HeapStringAppend(appoggio, "\t");
            }
            HeapStringAppend(appoggio, args[currentIndex]);
            number_param--;
            currentIndex++;
            if (number_param == 0) {
                strcpy(options[index_in_options].value, HeapStringGetRaw(appoggio));
                state = STATE_ACCEPT_OPT_TOKEN;
                HeapStringSet(appoggio, "");
            }
        }
        if (currentIndex == num && state != STATE_END_BAD) {
            state = STATE_END_GOOD;
        }
    }
    DisposeHeapString(appoggio);
    return state == STATE_END_GOOD ? SITH_RET_OK : SITH_RET_ERR;
}

int ReadConfigFile(void* mask) {
    File* configFile = CreateFileObject(config, SITH_FS_ZERO, SITH_FILEMODE_RO, SITH_OPENMODE_EXIST, 0);
    if (configFile == NULL) {
        if (SITH_E_COMPARE(GetErrorCode(), SITH_E_NOTFOUND)) {
            printf("No config file to read\n");
            fflush(stdout);
            SetErrorCode(SITH_E_NONE);
            return SITH_RET_ERR;
        }
        else {
            HandleErrorStatus("Error opening config file");
            return SITH_RET_ERR;
        }
    }

    // Estimate content length
    FileSize fsize;
    int error = GetFileObjectSize(configFile, &fsize);
    if (error) {
        HandleErrorStatus("Error getting file size");
        CloseFileObject(configFile);
        return SITH_RET_ERR;
    }

    size_t fs = (size_t) SITH_FS_LL(fsize);
    char* content = calloc(fs + 1, sizeof (char));
    if (content == NULL) return SITH_RET_ERR;
    error = ReadFromFileObject(configFile, content, &fs);
    if (error) {
        HandleErrorStatus("Error reading config file");
        free(content);
        CloseFileObject(configFile);
        return SITH_RET_ERR;
    }
    CloseFileObject(configFile);
    HeapString* par = CreateHeapString(content);
    free(content);
    if (par == NULL) {
        HandleErrorStatus("Could not create temporary strings");
        return SITH_RET_ERR;
    }
    LinkedList* list = CreateLinkedList();
    if (list == NULL) {
        DisposeHeapString(par);
    }
    HeapStringRemoveEndTokens(par);
    int res = HeapStringSplitAtChar(par, '\n', list);
    DisposeHeapString(par);
    if (res <= 0) {
        HandleErrorStatus("The file is not a valid configuration file");
        return SITH_RET_ERR;
    }
    unsigned short* mask_raw = calloc(optionCount, sizeof (unsigned short));
    if (mask_raw == NULL) {
        DestroyLinkedList(list);
        return SITH_RET_ERR;
    }
    for (size_t in = 0; in < (size_t) res; in++) {
        HeapString* string = (HeapString*) PopHeadFromList(list);
        //   printf("Parse %s \n", HeapStringGetRaw(string));
        HeapStringRemoveEndTokens(string);
        HeapString* opt = HeapStringSplitAtCharFirst(string, '=');
        if (opt == NULL) {
            char* st = HeapStringInner(string);
            printf("Corrupted line detected in config file, discarding \"%s\"\n", st);
            free(st);
            continue;
        }
        //  printf("Parse %s      \n", HeapStringGetRaw(string));
        for (size_t i = 0; i < optionCount; i++) {
            if (strcmp(options[i].config_symbol, "") == 0) {
                continue;
            }
            if (HeapStringCompareRaw(opt, options[i].config_symbol) == SITH_RET_OK) {
                if (HeapStringCompareRaw(string, options[i].value) == SITH_RET_OK) continue;
                mask_raw[i] = 1;
                if (options[i].number_args == 0) {
                    strcpy(options[i].value, HeapStringGetRaw(string));
                    break;
                }
                else {
                    strcpy(options[i].value, HeapStringGetRaw(string));
                    break;
                }
            }
        }
        DisposeHeapString(string);
        DisposeHeapString(opt);
    }
    DestroyLinkedList(list);
    if (mask_raw != NULL && masker != NULL) {
        masker(mask, mask_raw, optionCount);
    }
    free(mask_raw);
    return SITH_RET_OK;
}

void PrintHelp() {
    printf("\n%s\n\nAvailable options:\n\n", preamble);
    for (size_t i = 0; i < optionCount; i++) {
        printf("  %-8c%s\n",
                options[i].symbol,
                options[i].description);
    }
    printf("\n");
}

void splitter(char* in, char** out, unsigned short opt_num) {
    HeapString* app = CreateHeapString(in);
    HeapString* opt = NULL;
    for (unsigned short i = opt_num - 1; i > 0; i--) {
        opt = HeapStringSplitAtCharFirst(app, '\t');
        if (opt != NULL) {
            DisposeHeapString(opt);
            opt = NULL;
        }
    }
    opt = HeapStringSplitAtCharFirst(app, '\t');
    if (opt != NULL) {
        *out = HeapStringInner(opt);
        DisposeHeapString(app);
    }
    else {
        *out = HeapStringInner(app);
    }
}

int get_opt_num(const char opt, unsigned short num_opt, char** ret) {
    for (size_t i = 0; i < optionCount; i++) {
        if (options[i].symbol == opt) {
            if (options[i].number_args <= 1) {
                char* ptr = calloc(strlen(options[i].value) + 1, sizeof (char));
                if (ptr == NULL) return SITH_RET_ERR;
                strcpy(ptr, options[i].value);
                *ret = ptr;
                return SITH_RET_OK;
            }
            if (options[i].number_args < num_opt) {
                errno = EINVAL;
                return SITH_RET_ERR;
            }
            if (num_opt == 0) num_opt = 1;
            if (strcmp(options[i].value, SITH_OPT_EMPTY) == 0) {
                char* ptr = calloc(1, sizeof (char));
                if (ptr == NULL) return SITH_RET_ERR;
                strcpy(ptr, SITH_OPT_EMPTY);
                *ret = ptr;
            }
            else {
                char* ptr = NULL;
                splitter(options[i].value, &ptr, num_opt);
                *ret = ptr;
            }
            return SITH_RET_OK;
        }
    }
    errno = EINVAL;
    return SITH_RET_ERR;
}

int GetOptionString(const char opt, unsigned short num_opt, char* ret) {
    char* ptr = NULL;
    int err = get_opt_num(opt, num_opt, &ptr);
    if (err == SITH_RET_ERR) return SITH_RET_ERR;
    strcpy(ret, ptr);
    free(ptr);
    return SITH_RET_OK;
}

int GetOptionInt(const char opt, unsigned short num_opt, int* ret) {
    char* ptr = NULL;
    int err = get_opt_num(opt, num_opt, &ptr);
    if (err == SITH_RET_ERR) return SITH_RET_ERR;
    err = getInteger(ptr, ret);
    free(ptr);
    return err == SITH_RET_OK ? SITH_RET_OK : SITH_RET_ERR;
}

int GetOptionUInt(const char opt, unsigned short num_opt, unsigned int* ret) {
    char* ptr = NULL;
    int err = get_opt_num(opt, num_opt, &ptr);
    if (err == SITH_RET_ERR) return SITH_RET_ERR;
    err = getUInteger(ptr, ret);
    free(ptr);
    return err == SITH_RET_OK ? SITH_RET_OK : SITH_RET_ERR;
}

int GetOptionUShort(const char opt, unsigned short num_opt, unsigned short* ret) {
    char* ptr = NULL;
    int err = get_opt_num(opt, num_opt, &ptr);
    if (err == SITH_RET_ERR) return SITH_RET_ERR;
    err = getUShort(ptr, ret);
    free(ptr);
    return err == SITH_RET_OK ? SITH_RET_OK : SITH_RET_ERR;
}

int GetOptionBool(const char opt, unsigned short num_opt, unsigned short* ret) {
    char* ptr = NULL;
    int err = get_opt_num(opt, num_opt, &ptr);
    if (err == SITH_RET_ERR) return SITH_RET_ERR;
    if (strcmp(ptr, SITH_OPT_TRUE) == 0) *ret = 1;
    else *ret = 0;
    free(ptr);
    return err == SITH_RET_OK ? SITH_RET_OK : SITH_RET_ERR;
}

//This doesn't handle partial update

int recombine_opt(char* value, unsigned short num_opt, char* sett, char** recombined_value) {
    HeapString* val = CreateHeapString(value);
    if (val == NULL) return SITH_RET_ERR;
    LinkedList* list = CreateLinkedList();
    if (list == NULL) return SITH_RET_ERR;
    HeapStringSplitAtChar(val, '\t', list);
    DisposeHeapString(val);
    long size = GetLinkedListSize(list);
    if (size < num_opt) {
        DestroyLinkedList(list);
        return SITH_RET_ERR;
    }
    HeapString* new_val = CreateHeapString("");
    if (new_val == NULL) {
        DestroyLinkedList(list);
        return SITH_RET_ERR;
    }
    for (size_t i = 0; i < (size_t) size; i++) {
        HeapString* ttt = (HeapString*) PopHeadFromList(list);
        if (ttt == NULL) {
            DestroyLinkedList(list);
            DisposeHeapString(new_val);
            return SITH_RET_ERR;
        }
        if (i == (num_opt - 1)) {
            HeapStringAppend(new_val, sett);
        }
        else {
            HeapStringAppendSafe(new_val, ttt);
        }
        if (i != (size_t) (size - 1)) HeapStringAppend(new_val, "\t");
    }
    DestroyLinkedList(list);
    *recombined_value = HeapStringInner(new_val);
    return SITH_RET_OK;
}

int set_opt_num(const char opt, unsigned short num_opt, char* set) {
    for (size_t i = 0; i < optionCount; i++) {
        if (options[i].symbol == opt) {
            if (options[i].number_args <= 1) {
                memset(options[i].value, 0, SITH_MAX_VALUE_LEN);
                strcpy(options[i].value, set);
                return SITH_RET_OK;
            }
            if (options[i].number_args < num_opt) {
                errno = EINVAL;
                return SITH_RET_ERR;
            }
            if (num_opt == 0) num_opt = 1;
            if (strcmp(options[i].value, SITH_OPT_EMPTY) == 0) {
                strcpy(options[i].value, set);
                return SITH_RET_OK;
            }
            char* ptr = NULL;
            if (recombine_opt(options[i].value, num_opt, set, &ptr) == SITH_RET_ERR) return SITH_RET_ERR;
            strcpy(options[i].value, ptr);
            free(ptr);
            return SITH_RET_OK;
        }
    }
    errno = EINVAL;
    return SITH_RET_ERR;
}

int SetOptionUInt(const char opt, unsigned short num_opt, unsigned int set) {
    char sett[SITH_MAX_VALUE_LEN] = {0};
    snprintf(sett, SITH_MAX_VALUE_LEN, "%u", set);
    if (set_opt_num(opt, num_opt, sett) == SITH_RET_ERR) return SITH_RET_ERR;
    return SITH_RET_OK;
}

int SetOptionUShort(const char opt, unsigned short num_opt, unsigned short set) {
    char sett[SITH_MAX_VALUE_LEN] = {0};
    snprintf(sett, SITH_MAX_VALUE_LEN, "%u", set);
    if (set_opt_num(opt, num_opt, sett) == SITH_RET_ERR) return SITH_RET_ERR;
    return SITH_RET_OK;
}

int SetOptionBool(const char opt, unsigned short num_opt, unsigned short set) {
    char sett[5] = {0};
    if (set == 0) {
        strcpy(sett, SITH_OPT_FALSE);
    }
    else {
        strcpy(sett, SITH_OPT_TRUE);
    }
    if (set_opt_num(opt, num_opt, sett) == SITH_RET_ERR) return SITH_RET_ERR;
    return SITH_RET_OK;
}

int SetOptionString(const char opt, unsigned short num_opt, char* set) {
    if (set_opt_num(opt, num_opt, set) == SITH_RET_ERR) return SITH_RET_ERR;
    return SITH_RET_OK;
}

void test_getter() {
    Option opt[3] = {
        {'c', "", 3, SITH_OPT_EMPTY, ""},
        {'u', "", 3, "Ciccio\tPaolo\t10", ""},
        {'l', "", 1, "Ciao", ""}
    };
    InitArguments(opt, 3, "", "", NULL);
    char* ptr = NULL;

    int err = get_opt_num('c', 1, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, SITH_OPT_EMPTY) == 0) printf("Ok get_opt_num('c', 1, &ptr ); \n");
    }

    err = get_opt_num('c', 2, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, SITH_OPT_EMPTY) == 0) printf("Ok get_opt_num('c', 2, &ptr );\n");
    }
    err = get_opt_num('c', 5, &ptr);
    if (err == SITH_RET_ERR) {
        printf("Ok get_opt_num('c', 5, &ptr );\n");
    }
    err = get_opt_num('u', 2, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, "Paolo") == 0) printf("Ok get_opt_num('u', 2, &ptr );\n");
    }
    err = get_opt_num('u', 3, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, "10") == 0) printf("Ok get_opt_num('u', 3, &ptr );\n");
    }
    err = get_opt_num('u', 1, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, "Ciccio") == 0) printf("Ok get_opt_num('u', 1, &ptr );\n");
    }
    err = get_opt_num('u', 0, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, "Ciccio") == 0) printf("Ok get_opt_num('u', 0, &ptr );\n");
    }
    err = get_opt_num('u', 5, &ptr);
    if (err == SITH_RET_ERR) {
        printf("Ok get_opt_num('u', 5, &ptr );\n");
    }
    err = get_opt_num('l', 1, &ptr);
    if (err == SITH_RET_OK) {
        if (strcmp(ptr, "Ciao") == 0) printf("Ok get_opt_num('l', 1, &ptr );\n");
    }

}

void test_setter() {
    Option opt[3] = {
        {'c', "", 3, SITH_OPT_EMPTY, ""},
        {'u', "", 3, "Ciccio\tPaolo\t10", ""},
        {'l', "", 1, "Ciao", ""}
    };
    InitArguments(opt, 3, "", "", NULL);
    set_opt_num('l', 1, "Brutto");
    if (strcmp(options[2].value, "Brutto") == 0) printf("Test1 OK\n");
    set_opt_num('c', 1, "Tana");
    if (strcmp(options[0].value, "Tana") == 0) printf("Test2 OK\n");
    set_opt_num('u', 3, "Kappa");
    char* ptr = NULL;
    get_opt_num('u', 3, &ptr);
    if (strcmp(ptr, "Kappa") == 0) printf("Test3 passed\n");
}

void test_splitter() {
    char* out = NULL;
    char* in = "127.0.0.1\t8000\t6000";
    splitter(in, &out, 1);
    printf("%s\n", out);
    free(out);
    splitter(in, &out, 2);
    printf("%s\n", out);
    free(out);
    splitter(in, &out, 3);
    printf("%s\n", out);
    free(out);
}

void test_split() {
    HeapString* pp = CreateHeapString("Kappa\t\t66\t");
    LinkedList* ll = CreateLinkedList();
    HeapStringSplitAtChar(pp, '\t', ll);
    if (GetLinkedListSize(ll) == 3) printf("OK Test SPlit passed\n");
}
