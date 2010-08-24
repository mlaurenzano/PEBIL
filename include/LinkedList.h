/* 
 * This file is part of the pebil project.
 * 
 * Copyright (c) 2010, University of California Regents
 * All rights reserved.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
