namespace LRUDistanceAnalyzer
{
	#define VOID void
	#define ADDR void *
	#define INFINITE (unsigned long int)-1
	#define OFFSET_SIZE 6
	#define BUFFER_SIZE 4194304 
	#define BIN_SIZE 32 
	#define HASH_TABLE_SIZE 999983
	#define BATCH_ALLOC_SIZE 1024*1024	
	void Init();
	VOID RecordMemAccess(VOID * addr,uint64_t* BBStats);
	void OutputResults();
}
