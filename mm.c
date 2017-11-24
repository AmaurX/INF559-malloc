/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Dexter",
    /* First member's full name */
    "Amaury Camus",
    /* First member's email address */
    "amaury.camus@polytechnique.edu",
    /* Second member's full name (leave blank if none) */
    "Gregoire Roussel",
    /* Second member's email address (leave blank if none) */
    "gregoire.roussel@polytechnique.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

int getStatusBit(int* metaWord);
void setStatusBit(int* metaWord, int status);
size_t getSize(int* metaWord);
void setSize(int* metaWord, size_t size);
int* getStartMeta(void* blockPointer);
int* getEndMeta(int* blockPointer);
bool setMetas(int* meta, int size, int status);
bool isPreviousFree(int* blockPointer, int* previousSize);
bool isNextFree(int* blockPointer, int* nextSize);

int *our_mm_malloc(size_t size);
int *our_mm_realloc(int *ptr, size_t size);
void our_mm_free(int *blockPtr);
void update_heap_end();
bool findFirstFreeSpace(size_t size, int* freeBlock);
bool mm_check();
void findBigestFreeSpace(int *size, int* freeBlock);


int *beginning;
size_t heap_size = 1<<10;
size_t add_block_size = 1<<10;
int *current_heap;
int *heap_end;

int totalAlloc = 0;
int numberOfFree = 0;

size_t WORD_SIZE = 4;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	beginning = (int*)mem_sbrk(heap_size);
	*beginning = -1;
	current_heap = beginning + 1;
	update_heap_end();
    return 0;
}

void update_heap_end()
{
	heap_end = (int*) ((void*)mem_heap_hi()-3);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	return (void*)our_mm_malloc(size);
}


int *our_mm_malloc(size_t size)
{

	mm_check();
    size_t newsize = ALIGN(size)/WORD_SIZE + 2;
	size_t newsizeInBytes = newsize * WORD_SIZE;
	int* block;
	block = (int*)-1;
	
	//printf("%p and %p", block, (void*)block);

	int* possibleFreeBlock = (int*) 0;
	if(findFirstFreeSpace(newsize, possibleFreeBlock))
	{
		setMetas(possibleFreeBlock, newsize, 1);
		return possibleFreeBlock + 1;
	}

	while(heap_size - (size_t)(current_heap - beginning)*WORD_SIZE < newsizeInBytes){
		void* allocation = mem_sbrk(newsizeInBytes);
		if (allocation == (void *)-1){
			printf("\n\nbeginning    : %p\n", beginning);
			printf("current_heap : %p\n", current_heap);
			printf("heap_end     : %p\n", heap_end);
			printf("heap_size    : %d\n", heap_size);
			printf("comparison : %d < %d ?\n", heap_size - (size_t)(current_heap - beginning)*WORD_SIZE, newsizeInBytes);
			int BiggestFoundSize = -1;
			int* freeBlock = 0;
			findBigestFreeSpace(&BiggestFoundSize, freeBlock);
			printf("Biggest free block found at %p with size %d\n", freeBlock, BiggestFoundSize);


			return NULL;
		}
		update_heap_end();
		heap_size = mem_heapsize();
	}
	
	if(heap_size - (size_t)(current_heap - beginning)*WORD_SIZE >= newsizeInBytes){
		block = current_heap;
		current_heap += newsize;	
	}
	
	else{
		printf("GROS NAZE \n");
	}
	
    if (block == (void *)-1)
	return NULL;
    else {
    	// Stocking the size in meta words as the number of word!
    	// It's ok because we always have a multiple of two words.
		setMetas(block, newsize, 1);
    	//*block = newsize;
		//int* endMeta = getEndMeta(block);
		//*endMeta = newsize;
		
        //setStatusBit(block, 1);
        //setStatusBit(endMeta, 1);
		//printf("%p and %p", block, block + 1);

        return (block + 1);
    }
}

/*
 * mm_free
 * blockPtr : address of the first block of data : ie address given to the client
 * meta is one block before.
 */
void mm_free(void *blockPtr)
{
	our_mm_free((int*) blockPtr);
}

void our_mm_free(int *blockPtr)
{
	//printf("\tfree %p", (int*)blockPtr);
	
	printf("FREE");

	mm_check();
	int* startMeta = blockPtr -1;
	int* endMeta = getEndMeta(startMeta);
	

	// IL FAUT CES DEUX LIGNES!!!!!!!
	//setStatusBit(startMeta, 0);
	//setStatusBit(endMeta, 0);

	
	

	printf("\t %p -> %p, %zu | %zu\n", startMeta, endMeta, getSize(startMeta), getSize(endMeta));
	// --- NEXT BLOCK ---
	//check next block in memory
	int* nextBlockMeta = endMeta + 1;
	
	printf("yo");

	if(nextBlockMeta != current_heap && getStatusBit(nextBlockMeta) == 0)
	{
	  	printf("\tcoalescing with next ");
		//free block -> need to coalesce
		int totalSize = getSize(startMeta) + getSize(nextBlockMeta);

		//SEGFAULT PAR ICI!!

		int* nextBlockEndMeta =  getEndMeta(nextBlockMeta);

		endMeta = nextBlockEndMeta;
		printf("%p -> %p\n (%zu)", nextBlockMeta, nextBlockEndMeta, getSize(nextBlockMeta));

		// Setting metas.
		setMetas(startMeta, totalSize, 0);
	}
	printf("yo2");


		// --- PREV BLOCK ---
	//check prev block in memory
	


	int* prevBlockEndMeta = startMeta - 1;
	if(prevBlockEndMeta != beginning && getStatusBit(prevBlockEndMeta) == 0)
	{

		//previus block is free -> coalescing
		int totalSize = getSize(prevBlockEndMeta) + getSize(startMeta);
		
		printf("total size = %d, prevblockSize = %d, current size = %d \n",totalSize, getSize(prevBlockEndMeta), getSize(startMeta) );

		startMeta = startMeta - getSize(prevBlockEndMeta);
		if(startMeta >= beginning)
		{
		setMetas(startMeta, totalSize, 0);	
		}
		printf("\t coalescing xith %p -> %p", startMeta, prevBlockEndMeta);
	}

	if(*beginning == -1 || *beginning > (int) startMeta)
	{
		*beginning = (int) startMeta;
	}

	if(startMeta + getSize(startMeta) == current_heap)
	{
		current_heap = startMeta;
	}
	//printf("\t endOf Coal %p -> %p ", startMeta, endMeta);
	//setStatusBit(startMeta, 0);
	//setStatusBit(endMeta, 0);
	printf("Fin Free");
	//  /
}

// Give size in WORDs.
bool findFirstFreeSpace(size_t size, int* freeBlock)
{

	int* currentPtr = (int*)*beginning;
	if(currentPtr == (int*)-1)
	{
		currentPtr = beginning + 1;
	}

	while(currentPtr < current_heap)
	{
		int available_size = getSize(currentPtr);
		if(available_size >= size && getStatusBit(currentPtr) == 0){
			freeBlock = currentPtr;
			return true;
		}
		currentPtr += available_size;
	}
	return false;
}


void findBigestFreeSpace(int* mysize, int* freeBlock)
{
	int* currentPtr = (int*)*beginning;
	int currentSize = 0;

	if(currentPtr == (int*)-1)
	{
		currentPtr = beginning + 1;
	}

	printf("%p\n", currentPtr);
	printf("%d free on %d", numberOfFree, totalAlloc + numberOfFree);

	while((void*)currentPtr < (void*)current_heap)
	{
		int available_size = getSize(currentPtr);
		//printf("available_size = %d\n and free = %d", available_size, getStatusBit(currentPtr));
		if(available_size > currentSize && getStatusBit(currentPtr) == 0)
		{
			printf("HELLO\n");

			currentSize = available_size;
			freeBlock = currentPtr;
		}
		currentPtr += available_size;
	}
	*mysize = currentSize;
}


bool mm_check()
{
	int* currentPtr = beginning + 1;
	int occupation = getStatusBit(currentPtr) ^ 1;
	numberOfFree = 0;
	totalAlloc = 0;
	while(currentPtr < current_heap)
	{
		int available_size = getSize(currentPtr);
		int newoccupation = getStatusBit(currentPtr);
		if(newoccupation ==0)
		{
			numberOfFree +=1;
		}
		else
		{
			totalAlloc +=1;
		}
		if(newoccupation == occupation && occupation == 0){
			printf("DEBUG : two succesive blocks are free\n");
			return false;
		}

		if(available_size != getSize(currentPtr + available_size - 1))
		{
			printf("DEBUG : Beginning and Ending meta not matching... \n");
			return false;
		}

		occupation = newoccupation;
		currentPtr += available_size;
	}


	return true;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
	printf("reaalloc");
	if(ptr == NULL)
	{
		return mm_malloc(size);
	}
	else if(size == 0){
		mm_free(ptr);
		return ptr;
	}
	return (void*)our_mm_realloc((int*)ptr, size);
}

int *our_mm_realloc(int *ptr, size_t size)
{
    int *oldptr = ptr - 1;
    int *newptr = oldptr;
    size_t copySize;
    
    size_t askedSize = (ALIGN(size))/WORD_SIZE + 2;
    
    size_t oldSize = getSize(ptr);
    
    int nextBlockSize = 0;
    
    if(isNextFree(oldptr, &nextBlockSize))
      {
	printf("Le block suivant est libre!!");
    	if(oldSize + nextBlockSize >= askedSize)
	  {
	    printf("cool, y a de la place!");
	    // First, allocate new size
	    newptr = oldptr;
	    setMetas(newptr, askedSize, 1);
	    
	    
	    // Free the remaining space
	    int remainingFreeSpace = oldSize + nextBlockSize - askedSize;
	    if(remainingFreeSpace> 1){
	      int* nextFreeMetaBlock = newptr + askedSize;
	      setMetas(nextFreeMetaBlock, remainingFreeSpace, 0);
	    }
	    return (newptr + 1);
	  }
      }
    /*
      else if (isPreviousFree(oldptr, &previousBlockSize))
      {
      if(oldSize + previousBlockSize >= askedSize)
      {
      // First, allocate new size
      newptr = oldptr - previousBlockSize;
      setMetas(newptr, askedSize, 1);
      
      // Free the remaining space
      int remainingFreeSpace = oldSize + previousBlockSize - askedSize;
      if(remainingFreeSpace> 1){
      int* nextFreeMetaBlock = newptr + askedSize;
      setMetas(nextFreeMetaBlock, remainingFreeSpace, 0);
      }
      return (void*)(newptr + 1);
      }
      }
    */
    
    
    newptr = our_mm_malloc(size);
    
    if (newptr == NULL)
      return NULL;
    
    copySize = getSize(oldptr)*WORD_SIZE;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr+1, copySize);
    our_mm_free(oldptr+1);
    
    return newptr;
}

bool setMetas(int* meta, int size, int status)
{	
  	if(status > 1){return false;}  
  	*meta = size;
  	int* endMeta = getEndMeta(meta);
  	*endMeta = size;
  
  	setStatusBit(meta, status);
  	setStatusBit(endMeta, status);
  
  return true;
}


bool isNextFree(int* blockPointer, int* nextSize)
{
	int* meta = blockPointer;
	int* nextMeta = meta + getSize(meta);
	if(nextMeta != current_heap && getStatusBit(nextMeta) == 0)
	{
		*nextSize = getSize(nextMeta);
		return true;
	}
	nextSize = 0;
	return false;
}


bool isPreviousFree(int* blockPointer, int* previousSize)
{
	int* previousMeta = getStartMeta(blockPointer) - 1;
	if(previousMeta != beginning && getStatusBit(previousMeta) == 0)
	{
		*previousSize = getSize(previousMeta);
		return true;
	}
	previousSize = 0;
	return false;
}


int getStatusBit(int* metaWord)
{
	return *metaWord & 1;
}

/**
 * Setting status of file
 * Status should be 0 or 1
 */
void setStatusBit(int* metaWord, int status)
{
	*metaWord = (( *metaWord & -2) | (status & 1));
}

/**
 * Return the read size in number of words. 
 * In metaWord, size was stored in number of words.
 */
size_t getSize(int* metaWord)
{
	return  (*metaWord & -2);
}

/**
 * setting the size bits
 */
void setSize(int* metaWord, size_t size)
{
	size_t truncatedSize = size & -2;
	int res = truncatedSize & getStatusBit(metaWord);
	*metaWord = res;
}

/**
 * return meta word of the start of the block.
 */
int* getStartMeta(void* blockPointer)
{
	return (int*)blockPointer;
}

/**
 * return data start as a void pointer
 */
void* getStartData(void* blockPointer)
{
	return (void*) ( (int*)blockPointer + 1);
}

/**
 * return meta word of the end of the block
 */
int* getEndMeta(int* blockPointer)
{	
	//printf("hello\n");
	//printf("begin adress = %p\n",meta );	
	//printf("size =  %d \n", getSize(meta));
	//printf("end adress   = %p\n",meta + getSize(meta) - 1);	
	return blockPointer + getSize(blockPointer) - 1;
}


