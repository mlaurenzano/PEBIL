/* This program is free software: you can redistribute it and/or modify
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

#ifndef _Vector_h_
#define _Vector_h_

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

    void clear(){
        numberOfElements = 0;
    }

    void sort(int (*comparator) (const void*, const void*)){
        qsort(elements, numberOfElements, sizeof(T), comparator);
    }

    Vector<T>* removeRep(int (*comparator) (const void*, const void*)){
        qsort(elements, numberOfElements, sizeof(T), comparator);
        Vector<T>* rem = new Vector<T>();
        for (int32_t i = numberOfElements - 1; i > 0; i--){
            if (comparator(&elements[i-1], &elements[i]) == 0){
                (*rem).append(remove(i-1));
            }
        }
        return rem;
    }

    bool isSorted(int (*comparator) (const void*, const void*)){
        for (uint32_t i = 0; i < numberOfElements-1; i++){
            if (comparator(&elements[i],&elements[i+1]) > 0){
                return false;
            }
        }
        return true;
    }

    uint32_t insertSorted(T elt, int (*comparator) (const void*, const void*)){
        append(elt);
        qsort(elements, numberOfElements, sizeof(T), comparator);
        return numberOfElements;
    }

    inline T* operator&(){ return elements; }
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
        fprintf(stdout, "Vector with %d/%d elements", numberOfElements, capacity);
        for (uint32_t i = 0; i < numberOfElements; i++){
            fprintf(stdout, "\telements[%d] = %d", i, elements[i]);
        }
    }

    ~Vector(){
        if (elements){
            delete[] elements;
        }
    }

};

#endif /* _Vector_h_ */

