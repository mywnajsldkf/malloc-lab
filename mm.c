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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// #define NEXT_FIT
#define EXPLICIT

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4     /* Word and header/footer size (bytes) */
#define DSIZE 8     /* Double word size(bytes) */
#define CHUNKSIZE (1<<12)   /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address P */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define SUCP(bp) (*(void **)(bp))
#define PREP(bp) (*(void **)(bp + WSIZE))

static void *heap_listp;
static void *free_listp;
#ifdef NEXT_FIT
static char *next_fit;
#endif

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
#ifdef EXPLICIT
   /* Create the initial empty heap */
   if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)  
   { 
    return -1;
   }
   PUT(heap_listp, 0);                              /* Alignment padding */
   PUT(heap_listp + (1*WSIZE), PACK(DSIZE*2, 1));   /* Prologue header */

   PUT(heap_listp + (2*WSIZE), (int)NULL);          /* Prologue SUCCESOR, 제일 처음 root 역할*/
   PUT(heap_listp + (3*WSIZE), (int)NULL);          /* Prologue PREDECCESSOR */

   PUT(heap_listp + (4*WSIZE), PACK(DSIZE*2, 1));   /* Prologue Footer */
   PUT(heap_listp + (5*WSIZE), PACK(0, 1));         /* Epilogue header*/
   free_listp = heap_listp + DSIZE;
#else
    /* Create the initial empty heap */
   if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)  
   { 
    return -1;
   }
   PUT(heap_listp, 0);                           /* Alignment padding */
   PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));  /* Prologue header */
   PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));  /* Prologue footer */
   PUT(heap_listp + (3*WSIZE), PACK(0, 1));      /* Epilogue header */
   heap_listp += (2*WSIZE);
#endif

#ifdef NEXT_FIT
    next_fit = heap_listp;
#endif
   /* Extend the empty heap with a free block of CHUNKSIZE bytes*/
   if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
   {
    return -1;
   }
    return 0;
}

void putFreeBlock(void *bp)
{
    PREP(bp) = NULL;
    SUCP(bp) = free_listp;
    PREP(free_listp) = bp;
    free_listp = bp;
}

void removeBlock(void *bp){
    // 첫번째 블록을 없앨 경우
    if (bp == free_listp)
    {
        PREP(SUCP(bp)) = NULL;
        free_listp = SUCP(bp);
    }
    // 앞, 뒤 모두 있을 때
    else {
        SUCP(PREP(bp)) = SUCP(bp);
        PREP(SUCP(bp)) = PREP(bp);
    }
}

/* extend_heap */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    /* Initialize free blocks header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));   /* Free block header */

    PUT(FTRP(bp), PACK(size, 0));   /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epilogue header */
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
   size_t asize;        /* Adjusted block size */
   size_t extendsize;   /* Amount to extend heap if no fit */
   char *bp;

   /* Ignore spurious requests*/
    if (size == 0)
    {
        return NULL;
    }

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
    {
        asize = 2*DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    /* search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize)
{
#ifdef NEXT_FIT
    /* Next fit search */
    char *old_bp = next_fit;

    /* Search from the rover to the end of list */
    for (next_fit = old_bp; GET_SIZE(HDRP(next_fit)) > 0; next_fit = NEXT_BLKP(next_fit))
    {
        if (!GET_ALLOC(HDRP(next_fit)) && (asize <= GET_SIZE(HDRP(next_fit))))
        {
            return next_fit;
        }
    }
    
    /* Search from start of list to old rover */
    for (next_fit = heap_listp; next_fit < old_bp; next_fit = NEXT_BLKP(next_fit))
    {
        if (!GET_ALLOC(HDRP(next_fit)) && (asize <= GET_SIZE(HDRP(next_fit))))
        {
            return next_fit;
        }        
    }
    return NULL;    /* no fit found*/
#else

    #ifdef EXPLICIT
    void *bp;

    for(bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCP(bp)){
        if (asize <= GET_SIZE(HDRP(bp)))
        {
            return bp;
        }
    }
    return NULL;

    #else
    void *bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
    #endif

#endif
}

static void place(void *bp, size_t asize)
{
#ifdef EXPLICIT    
    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        removeBlock2(bp);
    }
    else
    {
        removeBlock(bp);
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
#else
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
#endif
}

void removeBlock2(void *bp)
{
    // 첫번째 블록을 없앨 때
    if (PREV_BLKP(bp) == free_listp)
    {
        PREP(SUCP(PREV_BLKP(bp))) = bp;
        free_listp = bp;
        SUCP(bp) = SUCP(PREV_BLKP(bp));
        PREP(bp) = NULL;
    }
    // 앞 뒤 모두 있을 때
    else
    {
        SUCP(PREP(PREV_BLKP(bp))) = bp;
        PREP(SUCP(PREV_BLKP(bp))) = bp;
        SUCP(bp) = SUCP(PREV_BLKP(bp));
        PREP(bp) = PREP(PREV_BLKP(bp));
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

#ifdef EXPLICIT
    if (prev_alloc && next_alloc)
    {
    }
    if (prev_alloc && !next_alloc)
    {
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if (!prev_alloc && !next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    putFreeBlock(bp);
    
#else
    if (prev_alloc && next_alloc)   // case1
    {
        return bp;
    }

    else if (prev_alloc && !next_alloc) // case2
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) // case 3
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    else {  // case4
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    #ifdef NEXT_FIT
    if ((next_fit) > (char *)bp && (next_fit < NEXT_BLKP(bp)))
    {
        next_fit = bp;
    }
    #endif

#endif

    return bp;
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
    //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
      copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}