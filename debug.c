#include "src/student_code.h"
#include <criterion/criterion.h>
#include <limits.h>
#include <stdio.h>

void* get_three_nodes() {
  void *memory = malloc(3 * sizeof(node_t));
  node_t* node0;
  node_t* node1;
  node_t* node2;

  node0 = memory;
  node1 = memory + sizeof(node_t);
  node2 = memory + 2 * sizeof(node_t);

  node0->size = 0;
  node0->is_free = true;
  node0->fwd = node1;
  node0->bwd = NULL;

  node1->size = 0;
  node1->is_free = true;
  node1->fwd = node2;
  node1->bwd = node0;

  node2->size = 0;
  node2->is_free = true;
  node2->fwd = NULL;
  node2->bwd = node1;

  return memory;
}

int main() {
  int size;
  int page_size = getpagesize();
  void *buff, *buff2, *buff3, *buff4;

  init(page_size);

  size=64;

  buff = mem_alloc(size);
  buff2 = mem_alloc(size);
  buff3 = mem_alloc(size);
  buff4 = mem_alloc(size);

  cr_assert(buff != NULL);
  cr_assert(buff2 != NULL);
  cr_assert(buff3 != NULL);
  cr_assert(buff4 != NULL);

  node_t *header = ((void*)buff) - sizeof(node_t);

  mem_free(buff);
  mem_free(buff2);
  mem_free(buff3);
  mem_free(buff4);

  //check to make sure the above seq. ends with
  //a single free node.
  cr_assert(header->size == page_size - sizeof(node_t));
  cr_assert(header->is_free == 1);
  cr_assert(header->bwd == NULL);
  cr_assert(header->fwd == NULL);

  destroy();
}
