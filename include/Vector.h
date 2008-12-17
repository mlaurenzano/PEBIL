#ifndef _Vector_h_
#define _Vector_h_

#include <Instruction.h>

#define DEFAULT_VECTOR_SIZE 16
#define DEFAULT_INCREASE_FACTOR 2

template <class T=uint32_t>
class Vector {
private:
    T* elements;
    uint32_t capacity;
    uint32_t numberOfElements;
    uint32_t sizeIncreaseFactor;

    void increaseCapacity(){
        capacity = capacity * sizeIncreaseFactor;
        T* dummy = new T[capacity];
        for (uint32_t i = 0; i < numberOfElements; i++){
            dummy[i] = elements[i];
        }
        delete[] elements;
        elements = dummy;
    }

public:
    Vector(uint32_t cap){
        capacity = cap;
        if (!capacity){
            capacity = DEFAULT_VECTOR_SIZE;
        }
        elements = new T[capacity];
        numberOfElements = 0;
        sizeIncreaseFactor = DEFAULT_INCREASE_FACTOR;
    }

    Vector(){
        capacity = DEFAULT_VECTOR_SIZE;
        elements = new T[capacity];
        numberOfElements = 0;
        sizeIncreaseFactor = DEFAULT_INCREASE_FACTOR;
    }

    T* operator&(){ return elements; }
    uint32_t getCapacity() { return capacity; }
    uint32_t size() { return numberOfElements; }
    bool empty(){ return (numberOfElements == 0); }

    T back() { ASSERT(numberOfElements); return elements[numberOfElements-1]; }
    T front() { ASSERT(numberOfElements); return elements[0]; }

    inline T& operator[] (uint32_t idx){
        ASSERT(idx < numberOfElements);
        return elements[idx];
    }

    T at(uint32_t idx){
        ASSERT(idx < numberOfElements);
        return elements[idx];
    }

    T remove(uint32_t idx){
        ASSERT(idx < numberOfElements);
        T rem = elements[idx];
        for (uint32_t i = idx; i < numberOfElements-1; i++){
            elements[i] = elements[i+1];
        }
        numberOfElements--;
        return rem;
    }

    void reverse(){
        T* elementsCopy = new T[capacity];
        for (uint32_t i = 0; i < numberOfElements; i++){
            elementsCopy[i] = elements[i];
        }
        for (uint32_t i = 0; i < numberOfElements; i++){
            elements[numberOfElements-i-1] = elementsCopy[i];
        }
        delete[] elementsCopy;
    }

    uint32_t append(T elt){
        if (numberOfElements == capacity){
            increaseCapacity();
        }
        numberOfElements++;
        assign(elt,numberOfElements-1);
        return numberOfElements;
    }

    uint32_t insert(T elt, uint32_t idx){
        ASSERT(idx <= numberOfElements); // we allow an insert after the last element
        if (numberOfElements == capacity){
            increaseCapacity();
        }
        if (numberOfElements){
            int32_t i = numberOfElements-1;
            int32_t sidx = (int32_t)idx;
            while (i >= sidx){
                elements[i+1] = elements[i];
                i--;
            }
        }
        elements[idx] = elt;
        numberOfElements++;
        return numberOfElements;
    }

    uint32_t assign(T elt, uint32_t idx){
        ASSERT(idx < numberOfElements);
        elements[idx] = elt;
    }

    void print(){
        PRINT_INFOR("Vector with %d/%d elements", numberOfElements, capacity);
        for (uint32_t i = 0; i < numberOfElements; i++){
            PRINT_INFOR("\telements[%d] = %d", i, elements[i]);
        }
    }

    ~Vector(){
        if (elements){
            delete[] elements;
        }
    }

};

static void testVector(){

    Vector<Operand*> v;
    Vector<Operand*>* p = new Vector<Operand*>();
    Operand* s = new Operand();
    s->setValue(3);
    v.append(s);
    p->append(s);
    v[0] = s;
    (*p)[0] = s;
    v[0]->setValue(2);
    p->at(0)->setValue(2);

    v.print();
    p->print();
    PRINT_INFOR("%p %p", v[0], (*p)[0]);
    PRINT_INFOR("%p", s);
    //    v[0]->setValue(2);
    delete v[0];

    Vector<uint32_t> a;
    a.append(3);
    a.append(6);
    a.append(9);
    a.print();

    a[1] = 5;
    a.print();

    delete p;
}


#endif /* _Vector_h_ */

