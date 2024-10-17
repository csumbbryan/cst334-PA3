#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>

#include "student_code.h"

int statusno, _initialized;

node_t *_chunklist;
void *_arena_start, *_arena_end;


void print_header(node_t *header){
  //Note: These printf statements may produce a segmentation fault if the buff
  //pointer is incorrect, e.g., if buff points to the start of the arena.
  printf("Header->size: %lu\n", header->size);
  printf("Header->fwd: %p\n", header->fwd);
  printf("Header->bwd: %p\n", header->bwd);
  printf("Header->is_free: %d\n", header->is_free);
}


int init(size_t size) {
  if(size > (size_t) MAX_ARENA_SIZE) {
    return ERR_BAD_ARGUMENTS;
  }


  // Find pagesize and increase allocation to match some multiple a page size
  // Question: Why is it good to match our allocation to the size of the page?
  int pagesize = getpagesize();

  if (pagesize <= 0)
    return ERR_CALL_FAILED;

  //Align to page size
  if( size % pagesize != 0 ) {
    // Calculate how much we need to increase to match the size of a page
    size -= size % pagesize;
    size += pagesize;
  }

  // Open up /dev/zero to zero-init our memory.
  int fd=open("/dev/zero",O_RDWR);
  if (fd == -1) {
    return ERR_SYSCALL_FAILED;
  }
  // Map memory from /dev/zero using mmap()
  _arena_start = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

  if (_arena_start == MAP_FAILED) {
    return ERR_SYSCALL_FAILED;
  }

  _arena_end = _arena_start + size;
  _initialized = 1;

  _chunklist = _arena_start;
  _chunklist -> size = size - sizeof(node_t);

  //  mmap sets the initial memory value
  // to zero so the fwd pointer of the is already null, but
  // it doesn't hurt to make that clear
  _chunklist -> fwd = NULL;
  _chunklist -> bwd = NULL;
  _chunklist -> is_free = true;

   return size;
}

int destroy() {

  if (_initialized == 0) {
    return ERR_UNINITIALIZED; 
  }

  // Remove arena with munmap()
  if(munmap(_arena_start, _arena_end - _arena_start) == -1) {
    return ERR_SYSCALL_FAILED;
  }

  // Question: Are there memory leaks here?

  // Clean up variables
  _arena_start = NULL;
  _arena_end = NULL;
  _chunklist = NULL;
  _initialized = 0;

  return 0;
}


node_t * find_first_free_chunk(size_t size, node_t* starting_node) {
  node_t* chunk = starting_node;
  while (chunk != NULL) {
    if (chunk->is_free && chunk->size >= size) {
      return chunk;
    }
    chunk = chunk->fwd;
  }
  return NULL;
}

void split_node(node_t* node, size_t size) {

  node_t* next = node->fwd;

  if (node->size == size){
    // Then the node is exactly the right size
    node->is_free = false;
  } else if(node->size - size < sizeof(node_t)){
    // Then the node is bigger than requested, but too small to split
    node->is_free = false;
  }
  else {
    // If the requested memory does not take up the entire free chunk, we need to
    // to split that chunk and add a new node to the free list.

    // create a new node after the allocated memory
    node_t* new_node = (void*) node + size + sizeof(node_t);
    // set the size of the node to the size of available memory,
    // removing the size to be allocated and the size of the node header
    new_node->size = node->size - size - sizeof(node_t);
    // set the new node backward pointer to the current node
    new_node->bwd = node;
    // set the new node forward pointer to the next node
    new_node->fwd = next;
    // new node should be free space
    new_node->is_free = true;
    // set the current node forward pointer to the new node
    node->fwd = new_node;
    // set the current node size to the size of memory that was allocated
    node->size = size;
    // set the current node to allocated (not free)
    node->is_free = false;
    // as long as the node wasn't the end of the list, set the next backward pointer to the new node
    if(next != NULL) {
      next->bwd = new_node;
    }
  }

  // Update header to correct size and state
  // todo
}

node_t* get_header(void* ptr) {
  if(ptr == NULL) {
    statusno = ERR_BAD_ARGUMENTS;
    return NULL;
  }
  return ptr - sizeof(node_t);
}

void coalesce_nodes(node_t* front, node_t* back) {
  if (front > back) {
    // Check to make sure they're in the right order
    statusno = ERR_BAD_ARGUMENTS;
    return;
  }
  if (front == back) {
    // Check to make sure they aren't the same node
    return;
  }
  if (front == NULL || back == NULL) {
    // Then one of them is already the end of the list
    statusno = ERR_BAD_ARGUMENTS;
    return;
  }
  if ( ! (front->is_free && back->is_free)) {
    // Then one of them isn't free
    statusno = ERR_CALL_FAILED;
    return;
  }

  // We want to do two things: skip over the second node and update size.
  front->size += back->size + sizeof(node_t);
  front->fwd = back->fwd;
  if(back->fwd != NULL) {
    back->fwd->bwd = front;
  }
}


void* mem_alloc(size_t size){

  // Check to make sure we are initialized, and if not set statusno and return NULL;
  if(_initialized == 0) {
    statusno = ERR_UNINITIALIZED;
    return NULL;
  }

  // Find a free chunk of memory
  node_t* node = NULL;
  node = find_first_free_chunk(size, _chunklist);

  // If finding a node returned NULL then we're out of memory
  if (node == NULL) {
    statusno = ERR_OUT_OF_MEMORY;
    return NULL;
  }

  // Split node to be the appropriate size, since there's no guarantee a free node is the right size already
  split_node(node, size);

  return (void*) node + sizeof(node_t);
}

void mem_free(void *ptr){

  if (ptr == NULL){
    statusno = ERR_BAD_ARGUMENTS;
    return;
  }

  if (ptr < _arena_start || ptr > _arena_end){
    // Then the pointer is outside of the arena
    statusno = ERR_BAD_ARGUMENTS;
    return;
  }

  // Step backward from the pointer to look at the node header
  node_t* node = get_header(ptr);

  // Free the memory
  node->is_free = true;

  // Coalesce together the chunks
  if(node->bwd == NULL && node->fwd == NULL) {
    // Node has already been coalesced into full memory space
    return;
  }
  if(node->bwd != NULL && node->bwd->is_free) {
    coalesce_nodes(node->bwd, node);
    if(node->fwd != NULL && node->fwd->is_free) {
      if(node->bwd->fwd == node->fwd) {
        coalesce_nodes(node->bwd, node->fwd);
        return;
      }
    }
  }
  if (node->fwd != NULL && node->fwd->is_free) {
    coalesce_nodes(node, node->fwd);
  }
  return;
}


