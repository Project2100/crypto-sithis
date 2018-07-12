/*
 * File:   list.h
 * Author: Project2100
 * Brief:  A loosely OO implementation of a linked list, along with some useful
 *          methods. It is NOT thread-safe, nor async-signal safe.
 *
 * Created on 31 January 2018, 14:33
 */


#ifndef SITH_LIST_H
#define SITH_LIST_H

#ifdef __cplusplus
extern "C" {
#endif

#define SITH_NOTFOUND 1
typedef struct sith_linkedlist LinkedList;


/**
 * Creates a new linked list. It is dynamically allocated
 *
 * @return A pointer to the new linked list
 */
LinkedList* CreateLinkedList();

/**
 * Adds the data pointed by 'data' to the end of the given list
 *
 * @param this
 * @param data
 * @return
 */
int AppendToList(LinkedList* list, void* data);

/**
 * Removes the first occurrence of the pointer 'data' in the given list
 *
 * @param this
 * @param data
 * @return
 */
int removeFirst(LinkedList* list, void* data);

/**
 * Applies the function pointed by 'function' to each element of the given list.
 * Depending on the 'mode' argument:
 *  - LLIST_SERIAL: Applications are done sequentially and in the order the
 *      elements are currently linked to the list
 *  - LLIST_PARALLEL: [UNIMPLEMENTED] Applications are concurrent
 *
 * @param this
 * @param function
 * @param parallel
 * @return
 */
int ApplyOnList(LinkedList* list, void (*function)(const void*));

/**
 * Empties the given list from its elements
 *
 * @param this the list to be emptied
 * @return
 */
int EmptyLinkedList(LinkedList* this);

/**
 * Frees all resources of a dynamically allocated linked list.
 * Do not call on statically allocated lists
 *
 * @param list
 * @return
 */
int DestroyLinkedList(LinkedList* list);

/**
 * Returns the length of the given list
 *
 * @param list
 * @return
 */
long GetLinkedListSize(LinkedList* list);

/**
 * Removes the head from the given list and returns its data pointer
 *
 * @param list
 * @return
 */
void* PopHeadFromList(LinkedList* list);


#ifdef __cplusplus
}
#endif

#endif /* SITH_LIST_H */
