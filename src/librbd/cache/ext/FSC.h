#include <stdio.h>
#include <stdlib.h>
//#include<conio.h>
#include<string.h>
#include "filetable.h"
#define BLOCK_SIZE 4096
#define BLOCK_CACHE_SIZE 5000
#define No_of_disk 5
#define CACHE_SIZE 120
#define SPURT_SIZE 2
//WAHT IS MAX_CACHE_SIZE
#define MAX_CACHE_SIZE	5000
#define WRITE_BACK_PERCENT 0.5
#define TRACE_FILE "TraceBlocksDA.csv"
#define THRESHOLD 0.5
#define Del_Threshold 0.5

typedef struct block
{
	int tag;
	char buffer[BLOCK_SIZE];
}block;

typedef struct blk
{
	int modify;
	int used;
	block *bptr;
}blk;

typedef struct cacheStripe
{
	int sno;
	int dirty;
	char partialparity[BLOCK_SIZE];
	blk block_array[No_of_disk];
	struct cacheStripe *prev;
	struct cacheStripe *next;
}cacheStripe;

typedef struct cacheQueue{
    int sno;
    struct cacheQueue *next;
    struct cacheQueue *prev;
}cacheQueue;

typedef struct cacheQueueParam{
    cacheQueue *front;
    cacheQueue *tail;
    int size; 
}cacheQueueParam;

int tag[BLOCK_CACHE_SIZE];
block block_cache[BLOCK_CACHE_SIZE];
cacheQueueParam *qp;//qp->size=0;
cacheStripe *hashTable[CACHE_SIZE];
char writeData[BLOCK_SIZE];


void initializeTag();
void initiatizeBlockCache();
void initializeQueueParam();
int getStripeNo(int blockno);
int getOperation();

/*Cache Read Operations*/
void cacheReadRequest(int stripeno, int blockno);
void placing(int stripeno, int blockno);
void cacheQueueUpdate(int stripeno);
void printCacheQueue();
void hashUpdate(int stripeno,int blockno);
int getBlockCacheIndex();
void denqueue();
int searchCache(int stripeno);
void updateCache(int stripeno);
void usedUpdateAtHash(int stripeno,int blockno);
void deleteClean();
void restoreBlocks(cacheStripe *s);
int delligibleStripe(int stripeno);
void deleteLRUelement();
void writeBack(int);
void policy_1();
void policy_2();

/*Cache Write Operations*/
void cacheWriteRequest(int stripeno, int blockno, char writedata[]);
void placingwrite(int stripeno, int blockno, char writedata[]);
void hashUpdateWrite(int stripeno, int blockno, char writedata[]);
void updatePartialParity(char *str1,char *str2, int len);
void hashUpdateWriteHit(int stripeno, int blockno, char writedata[]);
void deleteWrite();

int policyno;
int cacheMiss=0;
int cacheHit=0;
int modifiedStripe=0;
int usedBlock=0;
int unnecessaryWrite=0;
int modThreshold;
int noiop=0;




