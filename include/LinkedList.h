#ifndef _LinkedList_h_
#define _LinkedList_h_

#include <Base.h>

template <class T=uint32_t>
class LinkedList {
private:
    typedef struct entry {
        struct entry* next;
        T data;
    } entry_t;

    entry_t* elements;
    uint32_t elementCount;

    void destroy(){
        entry_t* ptr = elements;
        while(ptr){
            entry_t* tmp = ptr;
            ptr = ptr->next;
            delete tmp;
        }
        elements = NULL;
        elementCount = 0;
    }

public:
    LinkedList() : elements(NULL),elementCount(0) { }
    ~LinkedList(){
        destroy();
    }
    T insert(T newEntry){
        entry_t* newNode = new entry_t;
        newNode->next = elements;
        newNode->data = newEntry;
        elements = newNode;
        elementCount++;
        return newNode->data;
    }
    T shift(){
        entry_t* toDelete = elements;
        ASSERT(toDelete && "Trying to delete an element from empty list");
        elements = toDelete->next;
        T ret = toDelete->data;
        delete toDelete;
        elementCount--;
        return ret;
    }
    bool empty(){
        ASSERT(elements || !elementCount);
        return (!elements);
    }
    void clear(){
        destroy();
        elementCount = 0;
    }
    void print(){
        PRINT_INFOR("List 0x%x with %d ele : ", this, elementCount);
        entry_t* ptr = elements;
        while(ptr){
            PRINT_INFOR("\t0x%x",ptr);
            ptr = ptr->next;
        }
        PRINT_INFOR("\n");
    }
    uint32_t size() { return elementCount; }
};

#endif
