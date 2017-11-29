/*
 * mm.c
 * 
 * This memory management program replaces malloc, free & realloc by custom functions
 * 
 * The approach used is the "implicit list". If TRY_EXPLICIT_LIST is set, driver will use an "explicit list" approach (less stable). 
 * 
 * =============
 * 0 - GENERAL
 * =============
 * 
 * The memory structure is segmented in words as following : 
 * 
 * 0		4		8		12		16		20		24		28		32		36		40
 * |INIT	|m_st	|blk	|blk2	|m_end	|m_st	|free1	|free2	|free3	|free4	|m_end
 * 					|///////////////|				|-------------------------------|
 * 					|occupied block	|				| free block					|
 * 
 * In the code, m_st is often referred as meta or metaStart, as well as m_end is often endMeta
 * 
 * To reduce the risk of errors, all functions manipulate int* pointers. 
 * Conversion from/to void* is made into mm_malloc, mm_free, mm_realloc, 
 * which act as simple wrappers around our_mm_malloc, our_mm_free & our_mm_realloc
 * 
 * ================
 * 1 - META WORDS
 * ================
 * An allocable block is surrounded by 2 words (4 bytes) : the meta words.  
 * Metas allows the program to navigate in the memory and identify free and occupied regions
 * A meta is represented as an (int). Informations are stored in the following way :
 *  -	meta & 1  = 1 if block is occupied, 0 if free
 * 	-	meta & -2 = size of the block in words (metas are included)
 * 
 * ==============
 * 2 - STRATEGY
 * ==============
 * 	a - MALLOC
 * Upon a new request, mm_malloc scans the memory to find a large enough free block.
 * If such a block doesn't exist, we request new memory pages using mem_sbrk.
 * Once the free region found, the driver allocates just the right amount of blocks, 
 * 		/!\ except if the resulting free region left would be smaller than 4 words
 * 			in that case, the free region is entirely used
 * 
 * 	b - FREE
 * after setting metas to "free", the driver attempts to merge it with the neighbouring free regions
 * 
 * 	c - REALLOC
 * When requested for an extension, realloc first checks if the existing block is already fitting the request
 * If no, it checks if the block is followed by a large enough free zone
 * If no, it copies the data to another place (found by malloc()), then free the initial zone
 * 
 * 	d - EXPLICIT FREE LIST
 * 
 * The preprocessor variable TRY_EXPLICIT_LIST can be used to pass to a mode of explicit list, curently not working.
 * the idea is the following. As we made sure that each free block is at least 4 words long, including the metas, 
 * we can add more info: NEXT and PREV
 * 
 * 0		4		8		12		16		20		24		28		32		36		40
 * |NEXT	|m_st	|blk	|blk2	|m_end	|m_st	|NEXT	|free2	|free3	|PREV	|m_end
 * 					|///////////////|				|-------------------------------|
 * |5 	   	|		|occupied block	|				| 0     |			 	|5  	|
 * 
 * NEXT is the number of word from m_st to the m_st of the next free block, and PREV to the previous free block.
 * Notice that the very first block gives the offset to the first free block, and the previous of the first free block goes back to
 * the very first block
 * If a free block is last in line, his next is 0. 
 * All offsets are relative to m_st, which is why PREV is 5 (5*4 = 20 -> m_st) in the example above
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
	"gregoire.roussel@polytechnique.edu"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define DIFF_PTRS_IN_WORD(ptr1, ptr2) (int)(((long)ptr1 - (long)ptr2) / WORD_SIZE)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//set to 1 for more detailed runs
#define VERBOSE 0
//used in personal logging functions
#define LOG_SIZE 1024

#define TRY_EXPLICIT_LIST 0

/**
 * Internal logging
 */
void glog(const char *format, ...);

/**
 * Elementary bit/word reading/manipulation functions
 */
//META WORDS

int getStatusBit(int *metaWord);
void setStatusBit(int *metaWord, int status);
size_t getSize(int *metaWord);
void setSize(int *metaWord, size_t size);

//REGIONS
int *getStartMeta(void *blockPointer);
int *getEndMeta(int *blockPointer);
bool setMetas(int *meta, int size, int status);
bool isPreviousFree(int *blockPointer, int *previousSize);

//EXPLORATION
bool isNextFree(int *blockPointer, int *nextSize);
int getNextFreeOffset(int *startMeta);
int getPreviousFreeOffset(int *startMeta);
void setNextFree(int *startMeta, int offset);
void setPreviousFree(int *startMeta, int offset);

/**
 * Core functions
 * our_mm_malloc, our_mm_free, our_mm_realloc do the "real" job, when called by their respective wrappers
 */
int *our_mm_malloc(size_t size);
int *our_mm_realloc(int *ptr, size_t size);
void our_mm_free(int *blockPtr);
void update_heap_end();
bool findFirstFreeSpace(size_t size, int **freeBlock);
bool mm_check();
void findBigestFreeSpace(int *size, int **freeBlock);
bool isMetaValid(int *meta);
bool findBestFreeSpace(size_t size, int** freeBlock);
bool putFreeBlockInFreeList(int* startMeta);
bool takeFreeBlockOutOfTheList(int* startMeta);
bool findFirstFreeSpaceInExplicitList(size_t size, int** freeBlock);


/**
 * size of the new page requested to mem_sbrk when memory looks full
 */
const size_t add_block_size = 1 << 12;

const size_t WORD_SIZE = 4;
int *beginning;
size_t heap_size = 1 << 8;
int *current_heap;
int *heap_end;

int totalAlloc = 0;
int numberOfFree = 0;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	beginning = (int *)mem_sbrk(heap_size);
	*beginning = 0;
	current_heap = beginning + 1;
	update_heap_end();
    return 0;
}

/**
 * mem_heap_hi gives the address of the last used byte
 * the function updates the link to the last word accordingly
 */
void update_heap_end()
{
	heap_end = (int *)((void *)mem_heap_hi() - 3);
}

/* 
 * mm_malloc - Wrapper
 */
void *mm_malloc(size_t size)
{
	void *allocatedPtr = (void *)our_mm_malloc(size);
	//glog("Allocated %d bytes at %p", size, allocatedPtr);
	return allocatedPtr;
}

/**
 * Allocates a free block of size "size"
 * 
 * @param size:size_t lentgh (in words) of the block to allocate
 * @return pointer to first usable word 
 */
int *our_mm_malloc(size_t size)
{

	//mm_check();
	size_t newsize = (ALIGN(size) / WORD_SIZE + 2);
	size_t newsizeInBytes = newsize * WORD_SIZE;
	int *block;
	block = (int *)-1;
	
	if (newsize < 4)
	{
		printf("Allocsize is small \n");
	}
	//printf("%p and %p", block, (void*)block);

	int* possibleFreeBlock = (int*) 0;

	bool isThereAFreeBlock = false;


	if(TRY_EXPLICIT_LIST)
	{
		isThereAFreeBlock = findFirstFreeSpaceInExplicitList(newsize, &possibleFreeBlock);
	}
	else
	{
		isThereAFreeBlock = findFirstFreeSpace(newsize, &possibleFreeBlock);
	}

	if(isThereAFreeBlock)
	{
		// First, take the freeblock out of the free list
		if (TRY_EXPLICIT_LIST)
		{
			takeFreeBlockOutOfTheList(possibleFreeBlock);
		}

		int leftOverSize = getSize(possibleFreeBlock) - newsize;
		if (leftOverSize < 4)
		{
			setMetas(possibleFreeBlock, getSize(possibleFreeBlock), 1);
		}
		else
		{
			setMetas(possibleFreeBlock, newsize, 1);
			setMetas(possibleFreeBlock + newsize, leftOverSize, 0);
			our_mm_free(possibleFreeBlock + newsize + 1);
		}

		return possibleFreeBlock + 1;
	}

	while (heap_size - (size_t)(current_heap - beginning) * WORD_SIZE < newsizeInBytes)
	{
		void *allocation = mem_sbrk(newsizeInBytes);
		if (allocation == (void *)-1)
		{
			printf("\n\nbeginning    : %p\n", beginning);
			printf("current_heap : %p\n", current_heap);
			printf("heap_end     : %p\n", heap_end);
			printf("heap_size    : %d\n", heap_size);
			printf("comparison : %d < %d ?\n", heap_size - (size_t)(current_heap - beginning) * WORD_SIZE, newsizeInBytes);
			int BiggestFoundSize = -1;
			int *freeBlock = (int *)0;
			findBigestFreeSpace(&BiggestFoundSize, &freeBlock);
			printf("Biggest free block found at %p with size %d\n", freeBlock, BiggestFoundSize);

			return NULL;
		}
		update_heap_end();
		heap_size = mem_heapsize();
	}
	
	if (heap_size - (size_t)(current_heap - beginning) * WORD_SIZE >= newsizeInBytes)
	{
		block = current_heap;
		current_heap += newsize;	
	}
	
	else
	{
		//being not polite helps programmer reach a true state of creativity
		printf("GROS NAZE \n");
	}
	
    if (block == (void *)-1)
	return NULL;
	else
	{
    	// Stocking the size in meta words as the number of word!
    	// It's ok because we always have a multiple of two words.
		setMetas(block, newsize, 1);

        return (block + 1);
    }
}

/*
 * mm_free - Wrapper
 * blockPtr : address of the first block of data : ie address given to the client
 * meta is one block before.
 */
void mm_free(void *blockPtr)
{
	our_mm_free((int *)blockPtr);
}

/**
 * core mm_free function
 * strategy is given on top of file
 */
void our_mm_free(int *blockPtr)
{
	//printf("\tfree %p", (int*)blockPtr);
	
	//printf("FREE\n");

	//mm_check();
	
	int *startMeta = blockPtr - 1;

	if (!isMetaValid(startMeta))
	{
		printf("Invalid meta to free...");
		return;
	}
	int *endMeta = getEndMeta(startMeta);

	//printf("\t %p -> %p, %zu | %zu\n", startMeta, endMeta, getSize(startMeta), getSize(endMeta));
	
	// --- NEXT BLOCK ---
	//check next block in memory
	if (endMeta < current_heap - 1 && getStatusBit(endMeta + 1) == 0)
	{
		int *nextBlockMeta = endMeta + 1;
	  	//printf("\tcoalescing with next ");
		//free block -> need to coalesce
		int totalSize = getSize(startMeta) + getSize(nextBlockMeta);

		//printf("total size = %d, nextblockSize = %d, current size = %d \n",totalSize, getSize(nextBlockMeta), getSize(startMeta) );

		if (abs(totalSize) > heap_size || totalSize <= 0)
		{
			printf("Probleme de size : heap size = %d \n", heap_size);
			return;
		}

		int *nextBlockEndMeta = getEndMeta(nextBlockMeta);

		endMeta = nextBlockEndMeta;
		//printf("%p -> %p (%zu) \n", nextBlockMeta, nextBlockEndMeta, getSize(nextBlockMeta));

		// Setting metas.
		setMetas(startMeta, totalSize, 0);
		glog("Coalescing with next %p", startMeta);
	};

	// --- PREV BLOCK ---
	//check prev block in memory
	if (startMeta > beginning + 1 && getStatusBit(startMeta - 1) == 0)
	{
		int *prevBlockEndMeta = startMeta - 1;
		//previus block is free -> coalescing
		int totalSize = getSize(prevBlockEndMeta) + getSize(startMeta);
		
		//printf("total size = %d, prevblockSize = %d, current size = %d \n",totalSize, getSize(prevBlockEndMeta), getSize(startMeta) );

		if (abs(totalSize) > heap_size || totalSize <= 0)
		{
			printf("Probleme de size...\n");
			return;
		}

		startMeta = startMeta - getSize(prevBlockEndMeta);
		if (startMeta > beginning)
		{
			glog("Coalescing with previous %p", startMeta);
			setMetas(startMeta, totalSize, 0);	
		}
		//printf("\t coalescing with previous %p -> %p \n", startMeta, prevBlockEndMeta);
	}

	setStatusBit(startMeta, 0);
	setStatusBit(endMeta, 0);

	if (startMeta + getSize(startMeta) == current_heap)
	{
		//printf("current_heap took from %p to %p\n", current_heap, startMeta);
		current_heap = startMeta;
	}
	//printf("\t endOf Coal %p -> %p ", startMeta, endMeta);
	else
	{
		if (TRY_EXPLICIT_LIST)
		{
			putFreeBlockInFreeList(startMeta);
		}
	}

	glog("Freed space at %p", startMeta);
	//

	//Now starting the explicit Free list
}

/*
 * mm_realloc - Wrapper
 * handles only basic opeations, otherwise call our_mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
	{
		return mm_malloc(size);
	}
	else if (size == 0)
	{
		mm_free(ptr);
		return ptr;
	}
	return (void *)our_mm_realloc((int *)ptr, size);
}

/**
 * core realloc function
 * strategy is given on top of file
 */
int *our_mm_realloc(int *ptr, size_t size)
{
    int *oldptr = ptr - 1;
    int *newptr = oldptr;
    size_t copySize;
    
	size_t askedSize = ((ALIGN(size)) / WORD_SIZE + 2);
    
    size_t oldSize = getSize(oldptr);
    
	if (oldSize >= askedSize)
	{
		return ptr;
	}

    int nextBlockSize = askedSize - oldSize;
    //int previousBlockSize = askedSize - oldSize;

	if (isNextFree(oldptr, &nextBlockSize))
    {		
    	if(oldSize + nextBlockSize >= askedSize)
	  	{
	    	//printf("There is room!\n");
	    	// First, allocate new size
	    	newptr = oldptr;
	    	setMetas(newptr, oldSize + nextBlockSize, 1);
	    
	   	 	// Free the remaining space
	    	int remainingFreeSpace = oldSize + nextBlockSize - askedSize;
			if (remainingFreeSpace > 3)
			{
				int *nextFreeMetaBlock = newptr + getSize(newptr);
	      		setMetas(nextFreeMetaBlock, remainingFreeSpace, 1);
				our_mm_free(nextFreeMetaBlock);
	    	}
			//if(!mm_check()){printf("Heap inconsistent\n");}
	    	return (newptr + 1);
	  	}
    }
    
    
    //printf("We had to add at the end... \n");
    newptr = our_mm_malloc(size);
    
    if (newptr == NULL)
      return NULL;
    
	copySize = (getSize(oldptr) - 2) * WORD_SIZE;
    if (size < copySize)
		copySize = 8 * (ALIGN(size) / WORD_SIZE);
	memcpy((void *)newptr, (void *)(oldptr + 1), copySize);

	our_mm_free(oldptr + 1);
	//if(!mm_check()){printf("Here is more shit...\n");}
    return newptr;
}

//////////////////////////////////////////////////////////////////////
//
// The set of helper functions
//
//////////////////////////////////////////////////////////////////////

// structure of a free block : Meta|Next|......|Prev|EndMeta
// Next and Previous are the number of word from Meta to Meta!

/**
 * insert a free block in the explicit list
 * links the previous and next free block accordingly
 * (explicit list)
 */
bool putFreeBlockInFreeList(int *startMeta)
{
	//printf("Begin putFreeBlock\n");
	//mm_check();
	int freesize = getSize(startMeta);
	if (freesize < 4)
	{
		printf("The free block is really too small : %d.\n", freesize);
		return false;
	}

	// There are four possibilities : adding the first one,  adding at the beginning, 
	// adding at the end, or adding in the middle of the list.

	if (*beginning == 0)
	{
		// This means that no free block is available, this is the first one.
		int firstFreeSpace = (int)(startMeta - beginning);
		if (firstFreeSpace > 0)
		{
			*beginning = firstFreeSpace;
			setPreviousFree(startMeta, firstFreeSpace);
			setNextFree(startMeta, 0);
			return true;
		}
		else
		{
			printf("firstFreeSpace has negative offset\n");
			return false;
		}
	}
	else
	{
		// This means there is already one free block. First we try to find one after this one.
		int *currentPtr = startMeta + freesize;
		int *nextFreeMeta = (int *)0;
		while (currentPtr < current_heap)
		{
			int size = getSize(currentPtr);
			if (getStatusBit(currentPtr) == 0)
			{
				nextFreeMeta = currentPtr;
				break;
			}
			currentPtr += size;
			if (size <= 0)
			{
				printf("size is not really positive... \n");
				return false;
			}
		}
		if (nextFreeMeta == (int *)0)
		{
			//It means there is no free block after this free block, so we put the Next at 0.
			setNextFree(startMeta, 0);

			// Lets go backward to the previous free block then... No other choice, for now.
			
			currentPtr = startMeta;
			int *previousFreeMeta = beginning;
			//printf("Starting countdown\n");
			while (currentPtr > beginning + 1)
			{	
				//printf("%p\n", currentPtr);
				int size = getSize(currentPtr - 1);
				if (getStatusBit(currentPtr - 1) == 0)
				{
					previousFreeMeta = currentPtr - size;
					break;
				}
				currentPtr -= size;
				if (size <= 0)
				{
					printf("size is not positive : %d... \n", size);
					return false;
				}
			}
			if (previousFreeMeta == beginning)
			{
				//This is not supposed to happen, this case should have been handled before...
				printf("*Beginning == %d but no free space found \n", *beginning);
				mm_check();
				printf("There are %d free blocks\n", numberOfFree);
				return false;
			}
			else
			{
				setPreviousFree(startMeta, DIFF_PTRS_IN_WORD(startMeta, previousFreeMeta));
				setNextFree(previousFreeMeta, DIFF_PTRS_IN_WORD(startMeta, previousFreeMeta));
				return true;
			}
		}
		else
		{
			// Here, we have a next free block, which will contain a Prev which points to the previous free space in line, that we need!
			if (isMetaValid(nextFreeMeta))
			{
				int *previousFreeMeta = nextFreeMeta - getPreviousFreeOffset(nextFreeMeta);
				
				setPreviousFree(startMeta, DIFF_PTRS_IN_WORD(startMeta, previousFreeMeta));
				setNextFree(startMeta, DIFF_PTRS_IN_WORD(nextFreeMeta, startMeta));
				setPreviousFree(nextFreeMeta, DIFF_PTRS_IN_WORD(nextFreeMeta, startMeta));

				if (previousFreeMeta != beginning)
				{
					setNextFree(previousFreeMeta, DIFF_PTRS_IN_WORD(startMeta, previousFreeMeta));
				}
				else
				{
					*beginning = DIFF_PTRS_IN_WORD(startMeta, previousFreeMeta);
				}
				return true;
			}
			else
			{
				printf("nextFreeMeta isn't valid... : %p %p -> %d\n", startMeta, nextFreeMeta, getSize(nextFreeMeta));
				return false;
			}
		}
	}
}

/**
 * remove a free block from the explicit list of free blocks
 * links the previous and the next free block together
 * (explicit list)
 */
bool takeFreeBlockOutOfTheList(int *startMeta)
{
	mm_check();
	int freesize = getSize(startMeta);
	if (freesize < 4)
	{
		printf("The free block is too small : %d.\n", freesize);
		return false;
	}

	// There are four possibilities : deleting the last one, deleting at the beginning
	// deleting at the end, or deleting in the middle of the list.

	int nextFreeOffset = getNextFreeOffset(startMeta);
	int previousFreeOffset = getPreviousFreeOffset(startMeta);

	if (nextFreeOffset != 0)
	{
		int *nextFreeBlock = startMeta + nextFreeOffset;

		if (previousFreeOffset != 0)
		{
			setPreviousFree(nextFreeBlock, getPreviousFreeOffset(nextFreeBlock) + previousFreeOffset);
			int *previousFreeBlock = startMeta - previousFreeOffset;
			if (previousFreeBlock != beginning)
			{
				setNextFree(previousFreeBlock, getNextFreeOffset(previousFreeBlock) + nextFreeOffset);
			}
			else
			{
				*beginning = getNextFreeOffset(previousFreeBlock) + nextFreeOffset;
			}
			return true;
		}
		else
		{
			//This shouldn't happen 
			printf("This shouldn't happen \n");
			return false;
		}
	}
	else
	{
		if (previousFreeOffset != 0)
		{
			int *previousFreeBlock = startMeta - previousFreeOffset;
			if (previousFreeBlock != beginning)
			{
				setNextFree(previousFreeBlock, 0);
			}
			else
			{
				*beginning = 0;
			}
			return true;
		}
		else
		{
			//This shouldn't happen 
			printf("This shouldn't happen \n");
			return false;
		}
	}
}

/**
 * get offset to the previous free block
 * (explicit list)
 */
int getPreviousFreeOffset(int *startMeta)
{
	int *previousFree = getEndMeta(startMeta) - 1;
	return *previousFree;
}

/**
 * get offset to the next free block
 * (explicit list)
 */
int getNextFreeOffset(int *startMeta)
{
	int *nextFree = startMeta + 1;
	return *nextFree;
}

/**
 * used for explicit lists
 * links free block to the previous free one
 * address of the previous one is given as an offset
 */
void setPreviousFree(int *startMeta, int offset)
{
	int *previousFree = getEndMeta(startMeta) - 1;
	*previousFree = offset;
}

/**
 * used with explicit list architecture
 * links free block to the next free one
 */
void setNextFree(int *startMeta, int offset)
{
	int *nextFree = startMeta + 1;
	*nextFree = offset;
}

/**
 * Consistency check on meta words
 */
bool isMetaValid(int *meta)
{
	int *endMeta = getSize(meta) + meta;
	//printf("%d\n",getSize(meta));
	if (meta <= beginning || meta >= heap_end)
	{
		return false;
	}
	if (endMeta <= beginning || endMeta >= heap_end)
	{
		return false;
	}
	if (endMeta <= meta)
	{
		return false;
	}
	return true;
}

/**
 * Basic space finder
 * finds first free blocks that is large enough to contains the requested size
 * Give size in WORDs.
 */
bool findFirstFreeSpace(size_t size, int **freeBlock)
{

	int* currentPtr = beginning + 1;

	while(current_heap > currentPtr)
	{
		int available_size = getSize(currentPtr);
		if (available_size >= size && getStatusBit(currentPtr) == 0)
		{
			*freeBlock = currentPtr;
			//printf("available_size = %d and free = %d at ptr = %p\n", available_size, getStatusBit(currentPtr), currentPtr);

			return true;
		}
		if (available_size <= 0)
		{
			return false;
		}
		currentPtr += available_size;
	}
	return false;
}

/**
 * find the optimal free block for the requested size
 * assume meta are formatted in a explicist list template
 */
bool findFirstFreeSpaceInExplicitList(size_t size, int** freeBlock)
{

	int* currentPtr = beginning + *beginning;
	if(current_heap == beginning)
	{
		return false;
	}

	while(current_heap > currentPtr && getStatusBit(currentPtr) == 0)
	{
		int available_size = getSize(currentPtr);
		if(available_size >= size){
			*freeBlock = currentPtr;
			//printf("available_size = %d and free = %d at ptr = %p\n", available_size, getStatusBit(currentPtr), currentPtr);

			return true;
		}
		int nextFreeBlockOffset = getNextFreeOffset(currentPtr);
		if(nextFreeBlockOffset <= 0){return false;}
		currentPtr += nextFreeBlockOffset;
	}
	return false;
}


/**
 * find optimal space available
 * attempts to find a fre block with the exact requested size, otherwise choose a bigger block
 */
bool findBestFreeSpace(size_t size, int** freeBlock)
{
	bool foundAtLeastOne = false;
	int currentSize = 0;
	int* currentPtr = beginning + 1;

	while ((void *)current_heap - (void *)currentPtr > 0)
	{
		int available_size = getSize(currentPtr);
		if (available_size >= size && (!foundAtLeastOne || currentSize > available_size) && getStatusBit(currentPtr) == 0)
		{
			*freeBlock = currentPtr;
			currentSize = available_size;
			foundAtLeastOne = true;
			//printf("available_size = %d and free = %d at ptr = %p\n", available_size, getStatusBit(currentPtr), currentPtr);
			if (available_size == size)
			{
				return true;
			}
		}
		if (available_size <= 0)
		{
			return false;
		}
		currentPtr += available_size;
	}
	if (foundAtLeastOne)
	{
		return true;
	}
	return false;
}

/**
 * find the biggest space available
 * this function isn't used by the driver : it served as a test
 */
void findBigestFreeSpace(int *mysize, int **freeBlock)
{
	int* currentPtr = beginning+1;
	int currentSize = 0;

	//printf("%p\n", currentPtr);
	//printf("%d free on %d\n", numberOfFree, totalAlloc + numberOfFree);

	while ((void *)currentPtr < (void *)current_heap)
	{
		int available_size = getSize(currentPtr);
		//printf("available_size = %d and free = %d\n", available_size, getStatusBit(currentPtr));
		if (available_size > currentSize && getStatusBit(currentPtr) == 0)
		{
			// There is Mr. "free space" ! Let's greet him !
			printf("HELLO\n");

			currentSize = available_size;
			*freeBlock = currentPtr;
			*mysize = currentSize;
		}
		if (available_size <= 0)
		{
			return;
		}
		currentPtr += available_size;
	}
}


/**
 * Heap consistency checker
 * Checks two things : 
 *  - if there are two adjacent free blocks
 *  - if the size is the same in the startmeta and in the endMeta for each block
 * It also counts the number of free blocks and of occupied blocks. Useful for debugging
 */ 
bool mm_check()
{
	int *currentPtr = beginning + 1;
	int occupation = getStatusBit(currentPtr) ^ 1;
	numberOfFree = 0;
	totalAlloc = 0;
	while (currentPtr < current_heap)
	{
		int available_size = getSize(currentPtr);
		int newoccupation = getStatusBit(currentPtr);
		if (newoccupation == 0)
		{
			numberOfFree += 1;
		}
		else
		{
			totalAlloc += 1;
		}
		if (newoccupation == occupation && occupation == 0)
		{
			printf("DEBUG : two succesive blocks are free\n");
			return false;
		}

		if (available_size != getSize(currentPtr + available_size - 1))
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

/**
 * setMetas enter the right size in both start and end metas, with the right first bit to indicate occupation
*/
bool setMetas(int *meta, int size, int status)
{	
	if (status > 1)
	{
		return false;
	}
	if (meta < beginning || (meta + size) > current_heap || (meta + size) < meta)
	{
		return false;
	}
  	*meta = size;
	int *endMeta = getEndMeta(meta);
  	*endMeta = size;
  
  	setStatusBit(meta, status);
  	setStatusBit(endMeta, status);
  
  return true;
}

/**
 * returns true if next block is free
 * handles border case (when the block is the last one)
 */
bool isNextFree(int *blockPointer, int *nextSize)
{
	int *meta = blockPointer;
	int *nextMeta = meta + getSize(meta);
	if (nextMeta != current_heap && getStatusBit(nextMeta) == 0)
	{
		*nextSize = getSize(nextMeta);
		return true;
	}
	else if (nextMeta == current_heap)
	{
		*nextSize = -1;
		return true;
	}
	*nextSize = 0;
	return false;
}

/**
 * returns true if previous block is free
 * NOT USED
 */
bool isPreviousFree(int *blockPointer, int *previousSize)
{
	int *previousMeta = getStartMeta(blockPointer) - 1;
	if (previousMeta != beginning && getStatusBit(previousMeta) == 0)
	{
		*previousSize = getSize(previousMeta);
		return true;
	}
	previousSize = 0;
	return false;
}

/**
 * return the status bit of a meta word
 */
int getStatusBit(int *metaWord)
{
	return *metaWord & 1;
}

/**
 * Setting status of file
 * Status should be 0 or 1
 */
void setStatusBit(int *metaWord, int status)
{
	*metaWord = ((*metaWord & -2) | (status & 1));
}

/**
 * Return the read size in number of words. 
 * In metaWord, size was stored in number of words.
 */
size_t getSize(int *metaWord)
{
	return  (size_t)(*metaWord & -2);
}

/**
 * setting the size bits
 */
void setSize(int *metaWord, size_t size)
{
	size_t truncatedSize = size & -2;
	int res = truncatedSize & getStatusBit(metaWord);
	*metaWord = res;
}

/**
 * return meta word of the start of the block.
 */
int *getStartMeta(void *blockPointer)
{
	return (int *)blockPointer;
}

/**
 * return data start as a void pointer
 */
void *getStartData(void *blockPointer)
{
	return (void *)((int *)blockPointer + 1);
}

/**
 * return meta word of the end of the block
 */
int *getEndMeta(int *blockPointer)
{	
	//printf("hello\n");
	//printf("begin adress = %p\n",meta );	
	//printf("size =  %d \n", getSize(meta));
	//printf("end adress   = %p\n",meta + getSize(meta) - 1);	
	return blockPointer + getSize(blockPointer) - 1;
}

/*
 * Personal logging function
 * deactivated for release
 */
void glog(const char *format, ...)
{
#if VERBOSE
        char out[LOG_SIZE];
        va_list argptr;
        va_start(argptr, format);
        vsprintf(out, format, argptr);
        va_end(argptr);
        printf("%s\n", out);
#endif
}
