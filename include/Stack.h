#ifndef _Stack_h_
#define _Stack_h_

template <class T=uint32_t>
class Stack {
private:
    uint32_t maxSize;
    T* elements;
    int32_t topIndex;
public:
    Stack(uint32_t size){
        maxSize = size;
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
        ASSERT((topIndex+1) < maxSize);
        elements[++topIndex] = elt;
    }
    T pop(){
        ASSERT(topIndex >= 0);
        return elements[topIndex--];
    }
    bool empty(){ 
        return (topIndex == -1);
    }
    T top(){
        return elements[topIndex];
    }
    void print(){
        PRINT_INFOR("topIndex:%d ",topIndex);
        PRINT_INFOR("maxSize :%d ",maxSize);
        PRINT_INFOR("\n");
    }
};

#endif
