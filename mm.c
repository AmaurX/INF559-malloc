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
#include <stdarg.h>

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
bool findFirstFreeSpace(size_t size, int** freeBlock);
bool mm_check();
void findBigestFreeSpace(int *size, int** freeBlock);
bool isMetaValid(int *meta);
bool findBestFreeSpace(size_t size, int** freeBlock);


int *beginning;
size_t heap_size = 1<<8;
size_t add_block_size = 1<<12;
int *current_heap;
int *heap_end;

int totalAlloc = 0;
int numberOfFree = 0;

size_t WORD_SIZE = 4;

#define LOG_SIZE 1024
#define VERBOSE 0
/*
 * Personal logging function
 * deactivated for release
 */
void glog(const char* format, ...){
#if VERBOSE
        char out[LOG_SIZE];
        va_list argptr;
        va_start(argptr, format);
        vsprintf(out, format, argptr);
        va_end(argptr);
        printf("%s\n", out);
#endif
}


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
	void* allocatedPtr = (void*)our_mm_malloc(size);
	//glog("Allocated %d bytes at %p", size, allocatedPtr);
	return allocatedPtr;
}


int *our_mm_malloc(size_t size)
{

	//mm_check();
    size_t newsize =  (ALIGN(size)/WORD_SIZE + 2);
	size_t newsizeInBytes = newsize * WORD_SIZE;
	int* block;
	block = (int*)-1;
	
	//printf("%p and %p", block, (void*)block);

	int* possibleFreeBlock = (int*) 0;
	if(findBestFreeSpace(newsize, &possibleFreeBlock))
	{
		int leftOverSize = getSize(possibleFreeBlock) - newsize;
		if(leftOverSize <= 4){
			setMetas(possibleFreeBlock, getSize(possibleFreeBlock), 1);
		}
		else{
			setMetas(possibleFreeBlock, newsize, 1);
			setMetas(possibleFreeBlock + newsize, leftOverSize, 0);
			our_mm_free(possibleFreeBlock + newsize + 1);
		}

		return possibleFreeBlock + 1;
	}

	while(heap_size - (size_t)(current_heap - beginning)*WORD_SIZE < newsizeInBytes){
		void* allocation = mem_sbrk(add_block_size);
		if (allocation == (void *)-1){
			printf("\n\nbeginning    : %p\n", beginning);
			printf("current_heap : %p\n", current_heap);
			printf("heap_end     : %p\n", heap_end);
			printf("heap_size    : %d\n", heap_size);
			printf("comparison : %d < %d ?\n", heap_size - (size_t)(current_heap - beginning)*WORD_SIZE, newsizeInBytes);
			int BiggestFoundSize = -1;
			int* freeBlock = (int*)0;
			findBigestFreeSpace(&BiggestFoundSize, &freeBlock);
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
	
	//printf("FREE\n");

	//mm_check();
	
	int* startMeta = blockPtr -1;

	if(!isMetaValid(startMeta)){
		printf("Invalid meta to free...");
		return;}
	int* endMeta = getEndMeta(startMeta);
	

	// IL FAUT CES DEUX LIGNES!!!!!!!
	setStatusBit(startMeta, 0);
	setStatusBit(endMeta, 0);

	//printf("\t %p -> %p, %zu | %zu\n", startMeta, endMeta, getSize(startMeta), getSize(endMeta));
	// --- NEXT BLOCK ---
	//check next block in memory

	if(endMeta < current_heap -1 && getStatusBit(endMeta + 1) == 0)
	{
		int* nextBlockMeta = endMeta + 1;
	  	//printf("\tcoalescing with next ");
		//free block -> need to coalesce
		int totalSize = getSize(startMeta) + getSize(nextBlockMeta);

		//printf("total size = %d, nextblockSize = %d, current size = %d \n",totalSize, getSize(nextBlockMeta), getSize(startMeta) );

		//SEGFAULT PAR ICI!!
		if(abs(totalSize) > heap_size || totalSize <= 0){
			printf("Probleme de size : heap size = %d \n",heap_size);
			return;
		}

		int* nextBlockEndMeta =  getEndMeta(nextBlockMeta);

		endMeta = nextBlockEndMeta;
		//printf("%p -> %p (%zu) \n", nextBlockMeta, nextBlockEndMeta, getSize(nextBlockMeta));

		// Setting metas.
		setMetas(startMeta, totalSize, 0);
		glog("Coalescing with next %p", startMeta);
	}
;


		// --- PREV BLOCK ---
	//check prev block in memory
	if(startMeta > beginning +1 && getStatusBit(startMeta - 1) == 0)
	{
		int* prevBlockEndMeta = startMeta - 1;
		//previus block is free -> coalescing
		int totalSize = getSize(prevBlockEndMeta) + getSize(startMeta);
		
		//printf("total size = %d, prevblockSize = %d, current size = %d \n",totalSize, getSize(prevBlockEndMeta), getSize(startMeta) );

		if(abs(totalSize) > heap_size || totalSize <= 0){
			printf("Probleme de size...\n");
			return;
		}


		startMeta = startMeta - getSize(prevBlockEndMeta);
		if(startMeta > beginning)
		{
			glog("Coalescing with previous %p", startMeta);
			setMetas(startMeta, totalSize, 0);	
		}
		//printf("\t coalescing with previous %p -> %p \n", startMeta, prevBlockEndMeta);
	}

	if(*beginning == -1 || *beginning > (int) startMeta)
	{
		*beginning = (int) startMeta;
	}

	if(startMeta + getSize(startMeta) == current_heap)
	{
		//printf("current_heap took from %p to %p\n", current_heap, startMeta);
		current_heap = startMeta;
	}
	//printf("\t endOf Coal %p -> %p ", startMeta, endMeta);
	//setStatusBit(startMeta, 0);
	//setStatusBit(endMeta, 0);
	glog("Freed space at %p", startMeta);
	//
}


bool isMetaValid(int *meta)
{
	int* endMeta = getSize(meta) + meta;
	//printf("%d\n",getSize(meta));
	if(meta <= beginning || meta >= heap_end){return false;}
	if(endMeta <= beginning || endMeta >= heap_end){return false;}
	if(endMeta <= meta){return false;}
	return true;

}


// Give size in WORDs.
bool findFirstFreeSpace(size_t size, int** freeBlock)
{

	int* currentPtr = (int*)*beginning;
	if(currentPtr == (int*)-1)
	{
	 currentPtr = beginning + 1;
	}

	while((void*)current_heap - (void*)currentPtr > 0)
	{
		int available_size = getSize(currentPtr);
		if(available_size >= size && getStatusBit(currentPtr) == 0){
			*freeBlock = currentPtr;
			//printf("available_size = %d and free = %d at ptr = %p\n", available_size, getStatusBit(currentPtr), currentPtr);

			return true;
		}
		if(available_size <= 0){return false;}
		currentPtr += available_size;
	}
	return false;
}

bool findBestFreeSpace(size_t size, int** freeBlock)
{
	bool foundAtLeastOne = false;
	int currentSize = 0;
	int* currentPtr = (int*)*beginning;
	if(currentPtr == (int*)-1)
	{
	 currentPtr = beginning + 1;
	}

	while((void*)current_heap - (void*)currentPtr > 0)
	{
		int available_size = getSize(currentPtr);
		if(available_size >= size && (!foundAtLeastOne || currentSize > available_size) && getStatusBit(currentPtr) == 0){
			*freeBlock = currentPtr;
			currentSize = available_size;
			foundAtLeastOne = true;
			//printf("available_size = %d and free = %d at ptr = %p\n", available_size, getStatusBit(currentPtr), currentPtr);
			if(available_size == size){
				return true;
			}
		}
		if(available_size <= 0){return false;}
		currentPtr += available_size;
	}
	if(foundAtLeastOne){return true;}
	return false;
}


void findBigestFreeSpace(int* mysize, int** freeBlock)
{
	//int* currentPtr = (int*)*beginning;
	int * currentPtr = beginning+1;
	int currentSize = 0;

	if(currentPtr == (int*)-1)
	{
		currentPtr = beginning + 1;
	}

	printf("%p\n", currentPtr);
	printf("%d free on %d\n", numberOfFree, totalAlloc + numberOfFree);

	while((void*)currentPtr < (void*)current_heap)
	{
		int available_size = getSize(currentPtr);
		printf("available_size = %d and free = %d\n", available_size, getStatusBit(currentPtr));
		if(available_size > currentSize && getStatusBit(currentPtr) == 0)
		{
			printf("HELLO\n");

			currentSize = available_size;
			*freeBlock = currentPtr;
			*mysize = currentSize;
		}
		if(available_size <= 0){return;}
		currentPtr += available_size;
	}

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

	//printf("number of free :%d and occupied ; %d \n", numberOfFree, totalAlloc);

	return true;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
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
    
    size_t askedSize = ((ALIGN(size))/WORD_SIZE + 2);
    
    size_t oldSize = getSize(oldptr);
    
	if(oldSize >= askedSize){return ptr;}

    int nextBlockSize = askedSize - oldSize;
    
	
    if(isNextFree(oldptr, &nextBlockSize))
    {
		/*
		//printf("Le block suivant est libre!!\n");
		if(nextBlockSize == -1)
		{	
			//This means that this block is the last one. We just need to add more space to the heap
			int addingSize = (askedSize - oldSize)*WORD_SIZE;
			printf("adding Size  : %zu", addingSize);
			if(addingSize<0){printf("chelou\n");}
			void* allocation = mem_sbrk(addingSize);
			if (allocation == (void *)-1){
				printf("\n new-old byte: %d\n",( askedSize - oldSize)*WORD_SIZE);
				printf("beginning    : %p\n", beginning);
				printf("current_heap : %p\n", current_heap);
				printf("heap_end     : %p\n", heap_end);
				printf("heap_size    : %d\n", heap_size);
				printf("comparison   : %d < %d ?\n", heap_size - (size_t)(current_heap - beginning)*WORD_SIZE, addingSize);
				int BiggestFoundSize = -1;
				int* freeBlock = (int*)0;
				findBigestFreeSpace(&BiggestFoundSize, &freeBlock);
				printf("Biggest free block found at %p with size %d\n", freeBlock, BiggestFoundSize);
				return NULL;
			}
			update_heap_end();
			heap_size = mem_heapsize();
			current_heap += addingSize;	
			setMetas(newptr, askedSize, 1);
			current_heap = newptr + getSize(newptr);
			if(!mm_check()){printf("Here is double shit\n");}
			

			return (newptr + 1);
		}
		*/
		
    	if(oldSize + nextBlockSize >= askedSize)
	  	{
	    	printf("cool, y a de la place!\n");
	    	// First, allocate new size
	    	newptr = oldptr;
	    	setMetas(newptr, askedSize, 1);
	    
	    
	   	 	// Free the remaining space
	    	int remainingFreeSpace = oldSize + nextBlockSize - askedSize;
	    	if(remainingFreeSpace> 1){
	      		int* nextFreeMetaBlock = newptr + getSize(newptr);
	      		setMetas(nextFreeMetaBlock, remainingFreeSpace, 1);
				our_mm_free(nextFreeMetaBlock);
	    	}
			//if(!mm_check()){printf("Here is shit\n");}
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
    
    //printf("We had to add at the end... \n");
    newptr = our_mm_malloc(size);
    
    if (newptr == NULL)
      return NULL;
    
    copySize = (getSize(oldptr) - 2)*WORD_SIZE;
    if (size < copySize)
      copySize = 8 * (ALIGN(size)/WORD_SIZE);
    memcpy((void*)newptr, (void*)(oldptr+1), copySize);


    our_mm_free(oldptr+1);
    //if(!mm_check()){printf("Here is more shit...\n");}
    return newptr;
}

bool setMetas(int* meta, int size, int status)
{	
  	if(status > 1){return false;} 
	if(meta < beginning || (meta + size) > current_heap || (meta + size) < meta){return false;} 
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
	else if(nextMeta == current_heap){
		*nextSize = -1;
		return true;
	}
	*nextSize = 0;
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
	//if(abs(*metaWord & -2) > 12000){printf("YOU MOTHERFUCKER! : %d", *metaWord & -2);}
	return  (size_t)(*metaWord & -2);
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


