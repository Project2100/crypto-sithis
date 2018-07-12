#include <stdlib.h>
#include <errno.h>

#include "list.h"
#include "plat.h"

struct sith_entry {
    void* data;
    struct sith_entry* next;
};

struct sith_linkedlist {
    struct sith_entry* first;
    struct sith_entry* last;
    unsigned int size;
};

LinkedList* CreateLinkedList() {
    LinkedList* this = calloc(1, sizeof (LinkedList));
    if (this == NULL) {
        return NULL;
    }

    return this;
}

int AppendToList(LinkedList* this, void* data) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
    // Allocate new element
    struct sith_entry* elem = calloc(1, sizeof (struct sith_entry));
    if (elem == NULL) {
        return SITH_RET_ERR;
    }

    //Insert new element
    elem->data = data;

    // Link it to the list
    if (this->size == 0) {
        this->first = elem;
    }
    else {
        this->last->next = elem;
    }
    this->last = elem;
    this->size++;

    return SITH_RET_OK;
}

int ApplyOnList(LinkedList* self, void (*function)(const void*)) {
    if (self == NULL || function == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    for (struct sith_entry* elem = self->first; elem != NULL; elem = elem->next) {
        function(elem->data);
    }

    return SITH_RET_OK;
}

int removeFirst(LinkedList* this, void* data) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    struct sith_entry* prec = NULL;
    for (struct sith_entry* elem = this->first; elem != NULL; prec = elem, elem = elem->next) {
        if (elem->data == data) {

            //Unlink and dealloc elem
            if (prec == NULL)
                this->first = elem->next;
            else prec->next = elem->next;

            if (elem == this->last) {
                this->last = prec;
            }


            this->size--;
            free(elem);

            return SITH_RET_OK;
        }
    }

    return SITH_NOTFOUND;
}




int EmptyLinkedList(LinkedList* this) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    struct sith_entry* elem = this->first;
    struct sith_entry* prev = NULL;
    while (elem != NULL) {
        prev = elem;
        elem = elem->next;
        free(prev);
    }

    this->first = NULL;
    this->last = NULL;
    this->size = 0;

    return SITH_RET_OK;
}

int DestroyLinkedList(LinkedList* this) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    struct sith_entry* elem = this->first;
    struct sith_entry* prev = NULL;
    while (elem != NULL) {
        prev = elem;
        elem = elem->next;
        free(prev);
    }
    free(this);

    return SITH_RET_OK;
}

long GetLinkedListSize(LinkedList* this) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    size_t size = this->size;

    return size;
}

void* PopHeadFromList(LinkedList* this) {
    if (this == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (this->size == 0) {
        errno = EINVAL;
        return NULL;
    }

    struct sith_entry* head = this->first;
    this->first = this->first->next;
    this->size--;

    void* data = head->data;
    free(head);

    return data;

}
