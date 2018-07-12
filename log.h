/* 
 * File:   log.h
 * Author: llde
 * Basic Logging facility
 * Created on 23/05/2018
 *  
 */
#include "string_heap.h"
#include "file.h"

/**
 * Write the specified content to the specified logfile 
 * @param log_path : the path of the log file to be used
 * @param content : The content that will be written in the log
 * @return SITH_OK if succesfull, SITH_ERR if error
 */
int WriteLog(const char* log_path, HeapString* content);
