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
int* getEndMeta(void* blockPointer);

void *beginning;
size_t heap_size = 1<<18;
size_t add_block_size = 1<<14;
void *current_heap;
void *heap_end;

size_t WORD_SIZE = 4;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	beginning = mem_sbrk(heap_size);
	current_heap = beginning + WORD_SIZE;
	heap_end = mem_heap_hi();
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize = ALIGN(size) + 2 * WORD_SIZE;
	
	int* block;
	block = (void *)-1;
	
	int count = 0;
	
	while(heap_size - (size_t)(current_heap - beginning) < newsize && count < 3){
		void* allocation = mem_sbrk(add_block_size);
		if (allocation == (void *)-1)
			return NULL;
		heap_end = mem_heap_hi();
		heap_size = mem_heapsize();
		count ++;		
	}
	
	if(heap_size - (size_t)(current_heap - beginning) >= newsize){
		block = current_heap;
		current_heap += newsize;	
	}
	
	else{
		printf("GROS NAZE \n");
	}
	
    if (block == (void *)-1)
	return NULL;
    else {
    
    	*block = newsize;

        setStatusBit(block, 1);
        setStatusBit(getEndMeta(block), 1);

        return (void *)((char *)block + WORD_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *blockPtr)
{
	/*
	int* startMeta = getStartMeta(blockPtr);
	int* endMeta = getEndMeta(blockPtr);

	//first thing: set status to free
	setStatusBit(blockPtr, 0);

	//check next block in memory
	int* nextBlockMeta = getEndMeta(blockPtr) + 1;
	if( !getStatusBit(nextBlockMeta))
	{}
		//free block need to coalesce
		
	
	//check prev block in memory
	int* prevBlockMeta = getStartMeta(blockPtr) -1;
	*/
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
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
	*metaWord = (( *metaWord & -2) | status);
}

/**
 * Return the read size in number of words. 
 * In metaWord, size was stored in number of bytes.
 */
size_t getSize(int* metaWord)
{
	return  (*metaWord & -2)/WORD_SIZE;
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
int* getEndMeta(void* blockPointer)
{	
	//printf("hello\n");
	int* meta = getStartMeta(blockPointer);
	printf("begin adress = %p\n",meta );	
	printf("size =  %d \n", getSize(meta));
	printf("end adress   = %p\n",meta + getSize(meta)/4 - 1);	
	return meta + getSize(meta)/4 - 1;
}

