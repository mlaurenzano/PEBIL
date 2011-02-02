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

#ifndef _PriorityQueue_h_
#define _PriorityQueue_h_

#define DEFAULT_HEAP_SIZE 32

template <class T=uint32_t, class P=double>
class PriorityQueue {
private:
     typedef struct entry {
        P prio;
        T data;
     } entry_t;

    /**
     * member allowedSize of PriorityQueue class
     * the number of items that can fit into the allocated heap space
     */
     uint32_t bufferCount;

    /**
     * member entries of PriorityQueue class
     * the heap used to implement this PriorityQueue
     */
     entry_t* entries;

     /** 
      * elementCount member of priority queue
      * number of valid elements in the queue
      */
     uint32_t elementCount;

    /** the priority of the element with minimum priority in 
      * the queue 
      */
    double minimumPriority;

    void doubleSize(){
        bufferCount = elementCount*2;
        entry_t* newarray = new entry_t[bufferCount+1];
        memset(newarray,0x0,sizeof(entry_t)*(bufferCount+1));

        memcpy(newarray,entries,sizeof(entry_t)*(elementCount+1));

        delete[] entries;
        entries = newarray;
    }

    void exchange(uint32_t i,uint32_t j){
        ASSERT((i <= elementCount) && (j <= elementCount));
        entry_t tmp = entries[i];
        entries[i] = entries[j];
        entries[j] = tmp;
    }

    inline uint32_t parent(uint32_t i) { return (i/2); }
    inline uint32_t left(uint32_t i) { return (2*i); }
    inline uint32_t right(uint32_t i) { return (2*i+1); }

    void heapify(uint32_t i){
        uint32_t l = left(i);
        uint32_t r = right(i);
        uint32_t minIdx = ((l <= elementCount) && (entries[l].prio < entries[i].prio)) ?  l : i;
        minIdx = ((r <= elementCount) && (entries[r].prio < entries[minIdx].prio)) ? r : minIdx;
        if(minIdx != i){
            exchange(i,minIdx);
            heapify(minIdx);
        }
    }

public:
    /**
     * constructor method of PriorityQueue class
     * @param size the number of items that can fit into the allocated heap space
     */
    PriorityQueue(uint32_t size) : elementCount(0),minimumPriority(0.0) {
        bufferCount = size++;
        entries = new entry_t[bufferCount+1];
        memset(entries,0x0,sizeof(entry_t)*(bufferCount+1));
    }

    /**
     * default constructor method of PriorityQueue class
     */
    PriorityQueue() : elementCount(0),minimumPriority(0.0) {
        bufferCount = 1;
        entries = new entry_t[2];
        memset(entries,0x0,sizeof(entry_t)*2);
    }

    /**
     * destructor method of PriorityQueue class
     */
    ~PriorityQueue(){
        delete[] entries;
    }

    /**
     * method isEmpty of PriorityQueue class
     * @return true if elementCount == 0
     */
    bool isEmpty(){ return (elementCount == 0); }

    /**
     * method size of PriorityQueue class
     * @return member elementCount
     */
    uint32_t size() { return elementCount; }

    P getMinPriority() { return minimumPriority; }

    /**
     * method findMin of PriorityQueue class
     * @return the data of the entry in the heap with the highest priority
     */
    T findMin() { ASSERT(!isEmpty() && "queue must be non-empty for this operation"); return entries[1].data; }

    /**
     * method deleteMin of PriorityQueue class
     * deletes the entry in the heap with the highest priority and rebalances the heap
     * @return the data of the entry in the heap with the highest priority
     */
    T deleteMin(P* p){
        ASSERT(elementCount >= 1);
        T retVal = entries[1].data;
        if(p){
            *p = entries[1].prio;
        }
        entries[1] = entries[elementCount];
        elementCount--;
        if(elementCount){
            heapify(1);
        }
        return retVal;
    }

    /**
     * method insert of PriorityQueue class 
     * insert an entry into this PriorityQueue
     * @param data the data of the entry being inserted
     * @param p the priority of the entry being inserted
     * @return the data that was inserted
     */
    T insert(T data, P p){
        if(elementCount == bufferCount){
            doubleSize();
        }

        ASSERT(elementCount < bufferCount);
        elementCount++;

        entries[elementCount].data = data;
        entries[elementCount].prio = p;

        int i = elementCount;
        while((i > 1) && (entries[parent(i)].prio > entries[i].prio)){
            exchange(i,parent(i));
            i = parent(i);
        }

        if(minimumPriority < p){
            minimumPriority = p;
        }

        return data;
    }

    /**
     * method print of PriorityQueue class
     * print information about the state of this PriorityQueue
     * @return void
     */
    void print(void (*pf)(char*,T)=NULL){
        char buffer[__MAX_STRING_SIZE];
        printf("Priority Queue: size %d/%d\n", elementCount, bufferCount);
        if(pf){
            for(uint32_t i=1;i<=elementCount;i++){
                buffer[0] = '\0';
                pf(buffer,entries[i].data);
                printf("[%6d] (%20.8f,%s)\n",i,entries[i].prio,buffer);
            }
        }
    }
};

#endif /* _PriorityQueue_h_ */

