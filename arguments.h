/*
 * File:   arguments.h
 * Author: llde
 *
 * This want to be an easy of use framework for manage commandline option.
 * The priority of the configuration is: commandline win over configuration file
 * This is the second version  of the arguments and configuration file system system
 *
 * Created on 20 January 2018, 22:22
 */

#ifndef SITH_ARGUMENT_H
#define SITH_ARGUMENT_H

#ifdef __cplusplus
extern "C" {
#endif


#define SITH_MAXCH_VOPT 3
#define MAX_OPT_NUM 2
#define SITH_MAX_CONFIG_LEN 20
#define SITH_MAX_VALUE_LEN 255
#define SITH_MAX_DESCRIPTION_LEN 255

#define SITH_OPT_TRUE "true"
#define SITH_OPT_FALSE "false"
#define SITH_OPT_EMPTY ""

#define SITH_NOARGS 1
#define SITH_MALFORMED_ARGS 2
#define SITH_UNINIT 3
#define SITH_ARGS_SUCCESS 0

#define SITH_TRUE 1
#define SITH_FALSE 0

typedef enum type {
    option_null = 0,
    option_int,
    option_unsigned_int,
    option_unsigned_short,
    option_string,
    option_bool,
    option_array,
} option_type;

typedef struct sith_option {
    char symbol;
    char config_symbol[SITH_MAX_CONFIG_LEN]; //If is empty the setting cannot be setted by config file
    unsigned short number_args;
    char value[SITH_MAX_VALUE_LEN]; 
    char description[SITH_MAX_DESCRIPTION_LEN];
} Option;


typedef void (*MaskPopulate)(void*, unsigned short*, size_t);

/*
 * Initialize the arguments system.
 * @param options: The Option array wich specifies the program options.
 * @param number : the options number.
 * @param preamble: a stri ng with the preamble.
 * @param path: the path to the configuration file
 * @param populate_mask: the mask function to populate mask.
 */
void InitArguments(Option* options, size_t number, char* preamble, char* path, MaskPopulate populate_mask);

int CheckForConfigurationFile();

int IntializeDefaultConfigurationFile();
/*
 * Read and parse the unmangled arguments buffer.
 * @param num: the number of options of the arguments buffer  (argc parameter for main)
 * @param args: the "unmangled" argument buffer (argv paramter from main)
 * Remarks : The function bypass the always present executable path in argv[0].
 * As it's outside the scope of adding "arguments"  to the program.
 * Manage argv[0] manually if you need too.
 * @Return SITH_OK if complete, SITH_ERR if termianted unexpectly
 */
int ParseArguments(int num, char** args);

/*
 * Read the configuration file and announce the modified params in the mask
 * @param mask: a mask in which there are setted the bits related to changning options if any.
 * This is setted only if mask != NULL, and masker was setted during initialization
 * @return SITH_OK if ok, SITH_ERR if error occurred.
 * Remarks: It's illegal to use a mask type different that the one used by the masker function implementation
 */
int ReadConfigFile(void* mask);

/*
 * Obtain as integer the current option of the specified option
 * @param opt: The option you want the value
 * @param num_opt: the value of the option, used for multivalue options.
 * @param ret, the return value as an integer.
 * @return SITH_OK if of, SITH_NOTFOUND opt doesn't exist, SITH_NONVALID not represntable as integer
 *
 */
int GetOptionInt(const char opt, unsigned short num_opt, int* ret);

/*
 * Obtain as unsigned integer the current option of the specified option
 * @param opt: The option you want the value
 * @param num_opt: the value of the option, used for multivalue options.
 * @param ret, the return value as an unsigned integer.
 * @return SITH_OK if of, SITH_NOTFOUND opt doesn't exist, SITH_NONVALID not represntable as integer
 *
 */
int GetOptionUInt(const char opt, unsigned short num_opt, unsigned int* ret);

/*
 * Obtain as unsigned short the current option of the specified option
 * @param opt: The option you want the value
 * @param num_opt: the value of the option, used for multivalue options.
 * @param ret, the return value as an unsigned short.
 * @return SITH_OK if of, SITH_NOTFOUND opt doesn't exist, SITH_NONVALID not represntable as a unsigned short
 *
 */
int GetOptionUShort(const char opt, unsigned short num_opt, unsigned short* ret);


/*
 * Obtain as integer the current option of the specified option
 * @param opt: The option you want the value
 * @param num_opt: the value of the option, used for multivalue options.
 * @param ret, 1 If "true", 0 If "false".
 * @return SITH_OK, SITH_NOTFOUND opt doesn't exist, SITH_NONVALID opt not represntable as bool
 */
int GetOptionBool(const char opt, unsigned short num_opt, unsigned short* ret);

/*
 * Obtain as integer the current option of the specified option
 * @param opt: The option you want the value
 * @param num_opt: the value of the option, used for multivalue options.
 * @param ret, the return value as string copy. The caller must provide enough data.
 * Undefined bheviour if  the value is longer then ret capacity.
 * @return SITH_OK if of, SITH_NOTFOUND if opt doesn't exist
 */
int GetOptionString(const char opt, unsigned short num_opt, char* ret);



/*
 * Set the current option of the specified value
 * @param opt: The option you want to set
 * @param num_opt: the value of the option, used for multivalue options.
 * @param in, the value to set
 * @return SITH_OK, SITH_NOTFOUND opt doesn't exist
 */
int SetOptionUShort(const char opt, unsigned short num_opt, unsigned short in);


/*
 * Set the current option of the specified value
 * @param opt: The option you want to set
 * @param num_opt: the value of the option, used for multivalue options.
 * @param in, the value to set
 * @return SITH_OK, SITH_NOTFOUND opt doesn't exist
 */
int SetOptionInt(const char opt, unsigned short num_opt, int in);


/*
 * Set the current option of the specified value
 * @param opt: The option you want to set
 * @param num_opt: the value of the option, used for multivalue options.
 * @param in, the value to set
 * @return SITH_OK, SITH_NOTFOUND opt doesn't exist
 */
int SetOptionUInt(const char opt, unsigned short num_opt, unsigned int in);

/*
 * Set the current option of the specified value
 * @param opt: The option you want to set
 * @param num_opt: the value of the option, used for multivalue options.
 * @param in, 1 If set "true", 0 If set "false".
 * @return SITH_OK, SITH_NOTFOUND opt doesn't exist, SITH_NOTFOUND if not found
 */
int SetOptionBool(const char opt, unsigned short num_opt, unsigned short in);

/*
 * Set the current option of the specified value
 * @param opt: The option you want to set
 * @param num_opt: the value of the option, used for multivalue options.
 * @param in, the return value as string copy.
 * If the caller string is bigger then SITH_MAX_VALUE, only part of the option is copied.
 * Undefined bheviour if  the value is longer then ret capacity.
 * @return SITH_OK if of, SITH_NOTFOUND if opt doesn't exist
 *
 */
int SetOptionString(const char opt, unsigned short num_opt, char* in);

/*
 * Print the help for the program
 */
void PrintHelp();

//test
void test_splitter();
void test_getter();
void test_setter();
void test_split();
#endif /* SITH_ARGUMENT_H */
