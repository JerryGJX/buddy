#include "buddy.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// #define NULL ((void *)0)

#define MAX_RANK 16 // warning: 1 base

#define MAX_PAGE_NUM (1 << (MAX_RANK - 1))

#define MEM_PAGE_SIZE 4096

struct buddy_node {
  int rank;  // for rank i, the size of the page is 2^i * 4KB
  int order; // the order of this page in the buddy list
  void *start_addr;
  struct buddy_node *prev_page;
  struct buddy_node *next_page;
};

struct buddy_list {
  struct buddy_node *head_page;
  int cnt;
};

struct status_node {
  int8_t ifAlloc;
  struct buddy_node *ptr;
};

struct status_list {
  struct status_node status[MAX_RANK + 1][MAX_PAGE_NUM];//1 base rank
};

struct buddy_list free_list[MAX_RANK + 1], alloc_list[MAX_RANK + 1];

struct status_list page_status;

//--------------------------------for buddy
// node---------------------------------------------------
struct buddy_node *init_buddy_node(int rank, int order, void *start_addr) {
  struct buddy_node *ptr =
      (struct buddy_node *) malloc(sizeof(struct buddy_node));
  ptr->rank = rank;
  ptr->order = order;
  ptr->start_addr = start_addr;
  ptr->next_page = NULL;
  return ptr;
}

//--------------------------------for buddy
// list---------------------------------------------------

void insert_front(struct buddy_list *targetList,
                  struct buddy_node *targetPage) {
  targetPage->prev_page = NULL;
  if (targetList->head_page != NULL) {
    targetList->head_page->prev_page = targetPage;
  }
  targetPage->next_page = targetList->head_page;
  targetList->head_page = targetPage;
  targetList->cnt++;
}

void remove_front(struct buddy_list *targetList) {
  if (targetList->head_page->next_page != NULL)
    targetList->head_page->next_page->prev_page = NULL;
  targetList->head_page = targetList->head_page->next_page;
  targetList->cnt--;
}

void remove_certain(struct buddy_list *targetList,
                    struct buddy_node *targetPage) {
  struct buddy_node *prev, *next;
  prev = targetPage->prev_page;
  next = targetPage->next_page;
  if (next != NULL)
    next->prev_page = prev;
  if (prev == NULL)
    targetList->head_page = next;
  else
    prev->next_page = next;
  targetList->cnt--;
}

//--------------------------------for status
// list--------------------------------------------------

void init_status(int rank, int order, struct buddy_node *ptr) {
  page_status.status[rank][order].ifAlloc = 0;
  page_status.status[rank][order].ptr = ptr;
}

void set_status(int rank, int order, int8_t status) {
  page_status.status[rank][order].ifAlloc = status;
}

int8_t get_status(int rank, int order) {
  return page_status.status[rank][order].ifAlloc;
}

int8_t get_buddy_status(int rank, int order) {
  return page_status.status[rank][order ^ 1].ifAlloc;
}

struct buddy_node *get_buddy_ptr(int rank, int order) {
  return page_status.status[rank][order ^ 1].ptr;
}

struct buddy_node *get_self_ptr(int rank, int order) {
  return page_status.status[rank][order].ptr;
}

//--------------------------------for buddy
// system------------------------------------------------

void *start_ptr;
int page_num, max_rank;

int8_t if_buddy_valid(int rank, int order) {
  if (rank > max_rank || rank < 1)
    return 0;
  else {
    int max_order = page_num / (1 << (rank - 1));
    if ((order ^ 1) <= max_order) { return 1; } else { return 0; }
  }
}

struct buddy_node *get_buddy_node(int wanted_rank) {
  if (wanted_rank > max_rank)
    return NULL;
  else {
    if (free_list[wanted_rank].cnt == 0) {
      struct buddy_node *ptr = get_buddy_node(wanted_rank + 1);
      if (ptr == NULL) {
        return NULL;
      }
      int order = 2 * (ptr->order);
      insert_front(&free_list[wanted_rank],
                   get_self_ptr(wanted_rank, order + 1));
      struct buddy_node *ret_ptr = get_self_ptr(wanted_rank, order);
      set_status(wanted_rank, order, 1);
      insert_front(&alloc_list[wanted_rank], ret_ptr);
      return ret_ptr;
    } else {
      struct buddy_node *ptr = free_list[wanted_rank].head_page;
      set_status(wanted_rank, ptr->order, 1);
      remove_front(&free_list[wanted_rank]);
      insert_front(&alloc_list[wanted_rank], ptr);
      return ptr;
    }
  }
}

void return_buddy_node(int rank, int order) {
  if (rank == max_rank){
    struct buddy_node *ptr = get_self_ptr(rank, order);
    remove_certain(&alloc_list[rank], ptr);
    set_status(rank, order, 0);
    insert_front(&free_list[rank], ptr);
    return;
  }
  struct buddy_node *ptr = get_self_ptr(rank, order);
  remove_certain(&alloc_list[rank], ptr);
  set_status(rank, order, 0);
  if (get_buddy_status(rank, order) == 1) {
    insert_front(&free_list[rank], ptr);
    return;
  } else {
    struct buddy_node *buddy_ptr = get_buddy_ptr(rank, order);
    remove_certain(&free_list[rank], buddy_ptr);
    set_status(rank, order ^ 1, 0);
    return_buddy_node(rank + 1, order / 2);
  }
}

int init_page(void *p, int pgcount) {
  start_ptr = p;
  page_num = pgcount;
  max_rank = 1;
  while (1 << (max_rank - 1) < page_num && max_rank <= MAX_RANK) {
    max_rank++;
  }

  for (int i = max_rank; i > 0; i--) {
    int buddy_page_size = 1 << (i - 1); //一个buddy node里有几个页
    int buddy_page_mem_size = buddy_page_size * MEM_PAGE_SIZE;
    int buddy_node_num = page_num / buddy_page_size;
    // if (buddy_node_num * buddy_page_size < page_num) {
    //   printf("[error] wrong page size\n");
    //   printf("[error] buddy_node_num = %d\n", buddy_node_num);
    //   printf("[error] buddy_page_size = %d\n", buddy_page_size);
    //   printf("[error] page_num = %d\n", page_num);
    //   exit(0);
    // }
    void *tmp_ptr = start_ptr;

    for (int j = 0; j < buddy_node_num; j++) {
      struct buddy_node *buddy_ptr = init_buddy_node(i, j, tmp_ptr);
      init_status(i, j, buddy_ptr);
      tmp_ptr += buddy_page_mem_size;
    }
  }

  insert_front(&free_list[max_rank], get_self_ptr(max_rank, 0));

  return OK;
}

void *alloc_pages(int rank) {
  if (rank < 1 || rank > max_rank)
    return (void *) (-EINVAL);
  struct buddy_node *ptr = get_buddy_node(rank);
  if (ptr == NULL)
    return (void *) (-ENOSPC);
  return ptr->start_addr;
}

int query_alloc_rank(void *p) {
  void *pointed_addr = p;
  int rank = 0;
  int order = 0;
  for (rank = 1; rank < max_rank; rank++) {
    int buddy_page_size = 1 << (rank - 1);
    int buddy_page_mem_size = buddy_page_size * MEM_PAGE_SIZE;
    int tmp = (int) (pointed_addr - start_ptr) % buddy_page_mem_size;
    if (tmp != 0) {
      rank = rank - 1;
      break;
    }
    order = (int) (pointed_addr - start_ptr) / buddy_page_mem_size;
    if (get_status(rank, order) == 1) {
      break;
    }
  }
  return rank;
}

int return_pages(void *p) {
  if (p == NULL || p < start_ptr || p > start_ptr + page_num * MEM_PAGE_SIZE)
    return (-EINVAL);
  int rank = query_alloc_rank(p);
  int order = (int) (p - start_ptr) / ((1 << (rank - 1)) * MEM_PAGE_SIZE);

  if (get_status(rank, order) == 0)
    return (-EINVAL);
  else {
    return_buddy_node(rank, order);
    return OK;
  }
}

int query_ranks(void *p) {
  if (p == NULL || p < start_ptr || p > start_ptr + page_num * MEM_PAGE_SIZE)
    return (-EINVAL);
  int rank = query_alloc_rank(p);
  return rank;
}

int query_page_counts(int rank) {
  if (rank < 1 || rank > max_rank)
    return (-EINVAL);
  int buddy_page_size = 1 << (rank - 1);
  int total_page = page_num / buddy_page_size;
  int free_num = free_list[rank].cnt;
  return free_num;
}
