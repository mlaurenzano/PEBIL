#ifndef _Stack_h_
#define _Stack_h_

#define STACK_GROWTH_FACTOR 2
#define DEFAULT_STACK_SIZE 32

template <class T=uint32_t>
class Stack {
private:
    T* elements;
    uint32_t maxSize;
    int32_t topIndex;

    void growStack(){
        T* newElts = new T[maxSize * STACK_GROWTH_FACTOR];
        memcpy(newElts, elements, maxSize * sizeof(T));
        delete[] elements;
        maxSize *= STACK_GROWTH_FACTOR;
        elements = newElts;
    }

public:
    Stack(){
        maxSize = DEFAULT_STACK_SIZE;
        elements = new T[size];
        topIndex = -1;
    }
    Stack(uint32_t size){
        maxSize = size;
        if (!maxSize){
            maxSize = DEFAULT_STACK_SIZE;
        }
        elements = new T[size];
        topIndex = -1;
    }
    ~Stack(){
        delete[] elements;
    }

    void clear(){
        topIndex = -1;
    }
    void push(T elt){
        if (topIndex + 1 >= maxSize){
            PRINT_INFOR("top %d, max %d", topIndex, maxSize);
            growStack();
            PRINT_INFOR("top %d, max %d", topIndex, maxSize);
        }
        ASSERT((topIndex + 1) < maxSize);
        elements[++topIndex] = elt;
    }
    T pop(){
        ASSERT(topIndex >= 0);
        return elements[topIndex--];
    }
    uint32_t size(){
        return topIndex+1;
    }
    bool empty(){ 
        return (topIndex == -1);
    }
    T top(){
        return elements[topIndex];
    }
    void print(){
        PRINT_INFOR("topIndex:%d\tmaxSize:%d", topIndex, maxSize);
    }
};

#endif
