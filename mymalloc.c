#define HEAP_FREE 0
#define HEAP_ALLOCED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0

struct heapItem {
  struct heapItem * next; /* where the next block is, 0 for no next block */
  unsigned int size; /* how many bytes are contained in this block, not including these 5 header bytes */
  unsigned char type; /* 0 = free, 1 = allocated */
};

struct {
  struct heapItem * first;
  struct heapItem * nextFree;
} heap;

struct heapItem * current;
struct heapItem * next;

void zx_mallinit(void) {
  heap.nextFree = heap.first = NULL;
}

void zx_sbrk(void *addr, unsigned int size) {
  if(addr == NULL || size < sizeof(struct heapItem)) {
    return;
  }

  next = (struct heapItem *)addr;
  next->next = NULL;
  next->size = size - sizeof(struct heapItem);
  next->type = HEAP_FREE;

  if(heap.first == NULL) {
    heap.nextFree = heap.first = next;
    return;
  }

  current = heap.first;

  /* Tack the new block onto the end of the linked list */
  while(current->next != NULL) {
    current = current->next;
  }

  current->next = next;
}

void *zx_malloc(unsigned int size) {
  unsigned int cleanedUp = FALSE;
  unsigned int temp = size + sizeof(struct heapItem);

  do {
    /* no free memory available. just quit */
    if(heap.nextFree == NULL) {
      return NULL;
    }

    /* find a suitable location the put the new data, starting at heap.nextFree */
    current = heap.nextFree;

    do {
      if(current->type == HEAP_FREE && current->size >= temp) {
        /* suitable location found, set up the headers then return the pointer */
        next = (struct heapItem *)((void*)current + temp);
        next->next = current->next;
        next->size = current->size - temp;
        next->type = HEAP_FREE;

        current->size = size;
        current->type = HEAP_ALLOCED;
        current->next = next;

        heap.nextFree = next;

        return (void *)current + sizeof(struct heapItem);
      }

      current = current->next;
    } while(current);

    /* if no suitable free position was found and the heap has already been cleaned up then fail */
    if(cleanedUp) {
      return NULL;
    }

    heap.nextFree = NULL;
    current = heap.first;

    /* Attempt to coalesce the free blocks together then try again, but only once */
    while(current != NULL) {
      next = current->next;

      if(current->type == HEAP_FREE) {
        if(heap.nextFree == NULL) {
          heap.nextFree = current;
        }

        if(
          next && next->type == HEAP_FREE &&
          (struct heapItem *)(((void *)current) + sizeof(struct heapItem) + current->size) == next
        ) {
          current->next = next->next;
          current->size += next->size + sizeof(struct heapItem);
          continue;
        }
      }

      current = next;
    }

    cleanedUp = TRUE;
  } while (1);
}

void zx_free(void *addr) {
  if(addr != NULL) {
    current = ((struct heapItem *)(addr - sizeof(struct heapItem)));
    current->type = HEAP_FREE;

    /* try to keep the next available free block as unfragmented as possible */
    if(NULL != heap.nextFree && heap.nextFree == current->next) {
      next = (struct heapItem *)(addr + current->size);

      if(next == heap.nextFree) {
        current->next = next->next;
        current->size += next->size + sizeof(struct heapItem);
        heap.nextFree = current;
      }
    }
  }
}

void *zx_realloc(void *p, unsigned int size) {
  void * newOne;
  unsigned int tempSize;
  unsigned int updateNextFree;

  /* if realloc'ing a null pointer then just do a malloc */
  if(p == NULL) {
    return zx_malloc(size);
  }

  current = (struct heapItem *)(p - sizeof(struct heapItem));

  next = current->next;

  /* Is the existing block adjacent to a free one with enough
    total space? if so then just resize it and return the existing block */
  if(
    next != NULL &&
    next->type == HEAP_FREE &&
    (struct heapItem *)(((void *)current) + sizeof(struct heapItem) + current->size) == next
  ) {
     /* get the total amount of memory available in this interval */
    tempSize = current->size + next->size;

    if(tempSize >= size) {
      tempSize -= size;

      /* if the nextFree block is the same one as the free one we're updating, update the pointer as well */
      updateNextFree = (next == heap.nextFree);

      /* remove the old free block from the linked list as we'll be making a new one */
      current->next = next->next;

      /* update the current block's size to its new value */
      current->size = size;

      next = (struct heapItem *)(((void *)current) + sizeof(struct heapItem) + size);
      next->next = current->next;
      next->size = tempSize;
      next->type = HEAP_FREE;

      current->next = next;

      if(updateNextFree) {
        heap.nextFree = next;
      }

      return p;
    }
  }

  /* attempt to allocate a new block of the necessary size, memcpy the data into it then free the old one */
  newOne = zx_malloc(size);

  /* if the malloc failed, just fail here as well */
  if(!newOne) {
    return NULL;
  }

  /* memcpy the data if necessary */
  tempSize = size;

  if(tempSize > current->size) {
    tempSize = current->size;
  }

  if(tempSize) {
    memcpy(newOne, p, tempSize);
  }

  /* free the old data */
  current->type = HEAP_FREE;

  /* return a pointer to the new data */
  return newOne;
}

void *zx_calloc(unsigned int num, unsigned int size) {
  unsigned int tot = num * size;
  void *temp = zx_malloc(tot);

  if(temp) {
    memset(temp, 0, tot);
  }

  return temp;
}

/* very simple implemention of mallinfo just for the sake of completeness */
void zx_mallinfo(unsigned int *total, unsigned int *largest) {
  unsigned int tot = 0;
  unsigned int biggest = 0;

  current = heap.first;

  while(current != NULL) {
    if(current->type == HEAP_FREE) {
      if(biggest < current->size) {
        biggest = current->size;
        tot += biggest;
      }
      else {
        tot += current->size;
      }
    }

    current = current->next;
  }

  *total = tot;
  *largest = biggest;
}

unsigned char fakeHeap[4000];

int main(int argc, char ** argv) {
  void * temp = NULL;
  void * temp2 = NULL;

  memset(&fakeHeap, 0, 4000);

  zx_mallinit();

  /* set up some memory for our fake heap to use */
  zx_sbrk(&fakeHeap, 4000);

  /* test 1. malloc size bigger than heap should return NULL */
  temp = zx_malloc(4000);

  fputs(temp == NULL ? "true\n" : "false\n", stdout);

  /* test 2. malloc smaller size should return the proper address with the fake heap */
  temp = zx_malloc(5);

  fputs(temp == &(fakeHeap[sizeof(struct heapItem)]) ? "true\n" : "false\n", stdout);

  zx_free(temp);


  /* test 3. Freeing the memory from the previous test then mallocing the same amount again should return the same address */
  temp = zx_malloc(5);

  fputs(temp == &(fakeHeap[sizeof(struct heapItem)]) ? "true\n" : "false\n", stdout);


  /* test 4. reallocing memory at the top of the heap to be larger should return the same address */
  temp2 = zx_realloc(temp, 6);

  fputs(temp == temp2 ? "true\n" : "false\n", stdout);


  /* test 5. reallocing memory at the top of the heap to be smaller should return the same address */
  temp = zx_realloc(temp2, 5);

  fputs(temp == temp2 ? "true\n" : "false\n", stdout);


  /* test 6. calloc memory should work */
  temp2 = zx_calloc(2, 3);

  fputs(temp2 != NULL ? "true\n" : "false\n", stdout);


  /* test 7. the result from calloc should be different */
  fputs(temp != temp2 ? "true\n" : "false\n", stdout);

  zx_free(temp2);

  zx_free(temp);

  return 0;
}
