#ifndef _SimpleHash_h_
#define _SimpleHash_h_

template <class T=uint64_t>
class SimpleHash {
private:
#define BUCKET_COUNT 991
    typedef struct entry {
        struct entry* next;
        uint64_t key;
        T value;
    } entry_t;

    uint32_t entryCount;
    entry_t* buckets[BUCKET_COUNT];

public:
    SimpleHash() {
        entryCount = 0;
        for(uint32_t i=0;i<BUCKET_COUNT;i++){
            buckets[i] = NULL;
        }
    }
    ~SimpleHash() {
        for(uint32_t i=0;i<BUCKET_COUNT;i++){
            entry_t* current = buckets[i];
            while(current){
                entry_t* tmp = current->next;
                delete current;
                current = tmp;
            }
        }
    }

    bool get(uint64_t key,T* value=NULL){
        uint64_t bucketIdx = key % BUCKET_COUNT;
        entry_t* current = buckets[bucketIdx];

        for(;current;current=current->next){
            if(current->key == key){
                if(value){
                    *value = current->value;
                }
                break;
            }
        }
        return (current ? true : false);
    }

    bool exists(uint64_t key,T value){
        uint64_t bucketIdx = key % BUCKET_COUNT;
        entry_t* current = buckets[bucketIdx];

        for(;current;current=current->next){
            if((current->key == key) && (current->value == value)){
                break;
            }
        }
        return (current ? true : false);
    }

    T* values(){
        if(!entryCount){
            return NULL;
        }
        T* ret = new T[entryCount];
        uint32_t idx = 0;
        for(uint32_t i=0;i<BUCKET_COUNT;i++){
            for(entry_t* current = buckets[i];
                current;current=current->next){
                ret[idx++] = current->value;
            }
        }
        ASSERT((entryCount == idx) && "Fatal: There is a problem with Hash size");
        return ret;
    }

    void insert(uint64_t key,T value){

        uint64_t bucketIdx = key % BUCKET_COUNT;
        entry_t* current = buckets[bucketIdx];

        for(;current;current=current->next){
            if(current->key == key){
                ASSERT(current->value == value);
                break;
            }
        }

        if(!current){
            entry_t* newEntry = new entry_t;            
            newEntry->key = key;
            newEntry->value = value;
            newEntry->next = buckets[bucketIdx];
            buckets[bucketIdx] = newEntry;
            entryCount++;
        }
    }
    uint32_t size() { return entryCount; }

    void print(){
        printf("Element Count : %d\n",entryCount);
        for(uint32_t i=0;i<BUCKET_COUNT;i++){
            printf("\tBucket %5d :{ ",i);
            for(entry_t* current = buckets[i];
                current;current=current->next){
                printf("%lld ",current->key);
            }
            printf("}\n");
        }
    }
};
#endif
