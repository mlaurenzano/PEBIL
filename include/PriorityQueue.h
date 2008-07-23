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

    /** the priority of the element with maximum priority in 
      * the queue 
      */
     double maximumPriority;

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
    PriorityQueue(uint32_t size) : elementCount(0),maximumPriority(0.0) {
        bufferCount = size++;
        entries = new entry_t[bufferCount+1];
        memset(entries,0x0,sizeof(entry_t)*(bufferCount+1));
    }

    /**
     * default constructor method of PriorityQueue class
     */
    PriorityQueue() : elementCount(0),maximumPriority(0.0) {
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

    P getMaxPriority() { return maximumPriority; }

    /**
     * method findMax of PriorityQueue class
     * @return the data of the entry in the heap with the highest priority
     */
    T findMin() { ASSERT(!isEmpty() && "queue must be non-empty for this operation"); return entries[1].data; }

    /**
     * method deleteMax of PriorityQueue class
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

        if(maximumPriority < p){
            maximumPriority = p;
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

/**
 * template class MikeyQueue
 * contains a binary heap implementation of the priority queue ADT
 */
template <class T=uint32_t, class P=double>
class MikeyQueue { 
private:

    typedef struct entry {
        P priority;
        T data;
    } entry_t;

    /**
     * member heap of MikeyQueue class
     * the heap used to implement this MikeyQueue
     */
    entry_t* heap;

    /**
     * member currentSize of MikeyQueue class
     * the number of items that are currently in the heap
     */
    uint32_t currentSize;

    /**
     * member allowedSize of MikeyQueue class
     * the number of items that can fit into the allocated heap space
     */
    uint32_t allowedSize;

    P maxPriority;

    /**
     * method moveEntry of MikeyQueue class
     * @param target the address of the target entry
     * @param source the address of the source entry
     * @return void
     */
    void moveEntry(entry_t* target, entry_t* source){
       (*target).priority = (*source).priority;
       (*target).data = (*source).data;
    }

    /**
     * method percolateDown of MikeyQueue class 
     * @param hole the index of the entry to percolate
     * @return void
     */
    void percolateDown(uint32_t hole){

        ASSERT(heap && "heap must be non-null");

        uint32_t child;
        entry_t tmp;
        moveEntry(&tmp,&heap[hole]);

        for ( ; hole * 2 <= currentSize; hole = child ) {
            child = hole * 2;
            if (child != currentSize && heap[child+1].priority < heap[child].priority)
                child++;
            if (heap[child].priority < tmp.priority)
                moveEntry(&heap[hole],&heap[child]);
            else
                break;
        }
        moveEntry(&heap[hole],&tmp);
    }

    /**
     * method increaseSize of MikeyQueue class
     * double the size of the space allocated for the heap
     * @return the new size of the space allocated for the heap
     */
    uint32_t increaseSize(){

        uint32_t newSize;
        entry_t* newHeap;

        if (allowedSize <= 1){
            newSize = DEFAULT_HEAP_SIZE + 1;
        } else {
            newSize = ((allowedSize - 1) * 2) + 1;
        }

        newHeap = (entry_t *)malloc(sizeof(entry_t) * newSize);

        for(uint32_t i = 1; i < allowedSize; i++){
            newHeap[i].priority = heap[i].priority;
            newHeap[i].data = heap[i].data;
        }

        free(heap);
        heap = newHeap;

        PRINT_DEBUG("Heap size updated from %d to %d", allowedSize, newSize);    
        allowedSize = newSize;

        return allowedSize;
    }

public:

    /**
     * default constructor method of MikeyQueue class
     */
    MikeyQueue() : heap(NULL),currentSize(0), allowedSize(1),maxPriority(0.0) {}

    /**
     * constructor method of MikeyQueue class
     * @param size the number of items that can fit into the allocated heap space
     */
    MikeyQueue(uint32_t size) : heap(NULL), currentSize(0) {
        allowedSize = size + 1;
        heap = (entry_t *)malloc(sizeof(struct entry) * allowedSize);
    }

    /**
     * destructor method of MikeyQueue class
     */
    ~MikeyQueue(){
        if (heap){ free(heap); }
    }
    /**
     * method isEmpty of MikeyQueue class
     * @return true if currentSize == 0
     */
    bool isEmpty(){ return (currentSize == 0); }

    /**
     * method size of MikeyQueue class
     * @return member currentSize
     */
    uint32_t size() { return currentSize; }

    /**
     * method findMax of MikeyQueue class
     * @return the data of the entry in the heap with the highest priority
     */
    T findMin() { ASSERT(!isEmpty() && "queue must be non-empty for this operation"); return heap[1].data; }

    /**
     * method deleteMax of MikeyQueue class
     * deletes the entry in the heap with the highest priority and rebalances the heap
     * @return the data of the entry in the heap with the highest priority
     */
    T deleteMin(P* p){

        ASSERT(heap && "heap must be non-null");
        ASSERT(!isEmpty() && "queue must be non-empty for this operation");

        moveEntry(&heap[0],&heap[1]);
        moveEntry(&heap[1],&heap[currentSize]);

        currentSize--;

        percolateDown(1);

        ASSERT(currentSize <= allowedSize);

        if(p){
            *p = heap[0].priority;
        }
        return heap[0].data;
    }

    /**
     * method insert of MikeyQueue class 
     * insert an entry into this MikeyQueue
     * @param data the data of the entry being inserted
     * @param p the priority of the entry being inserted
     * @return the data that was inserted
     */
    T insert(T data, P p){

        ASSERT(currentSize <= allowedSize);

        if (currentSize + 1 == allowedSize){
            increaseSize();
        }

        int hole = ++currentSize;
        heap[0].priority = p;
        heap[0].data = data;

        for( ; (p < heap[hole/2].priority) && hole; hole /= 2){
            moveEntry(&heap[hole],&heap[hole/2]);
        }

        heap[hole].priority = p;
        heap[hole].data = data;

        if(maxPriority < p){
            maxPriority = p;
        }
        return data;
    }
    P getMaxPriority() { return maxPriority; }

    /**
     * method print of MikeyQueue class
     * print information about the state of this MikeyQueue
     * @return void
     */
    void print(void (*pf)(char*,T)=NULL){
        PRINT_INFOR("Priority Queue: size %d/%d", currentSize, allowedSize);
        for (uint32_t i = 1; i <= currentSize; i++){
            PRINT_INFOR("\theap[%d].priority %f", i, heap[i].priority);
        }
    }

};

#endif /* _PriorityQueue_h_ */

