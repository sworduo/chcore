#pragma once

#include <common/types.h>
#include <common/list.h>

/*
 * Supported Order: [0, BUDDY_MAX_ORDER).
 * The max allocated size (continous physical memory size) is
 * 2^(BUDDY_MAX_ORDER - 1) * 4K, i.e., 16M.
 */
#define BUDDY_PAGE_SIZE     (0x1000)
#define BUDDY_MAX_ORDER     (14UL)

/*
 `struct page` is the metadata of one physical 4k page. 
通过与start_page地址相减来判断当前page是第几个page
*/
struct page {
	/* Free list 存放当前物理块在free_list中的地址，当确定块地址时，其在链表中的位置也随之确定，因此可以快速访问和删除*/
	//node不是指针的好处在于，node的地址和其他变量的地址是挨在一起的，所以可以通过node字段的地址定位page结构体的地址。
	struct list_head node;
	/* Whether the correspond physical page is free now. */
	int allocated;
	/* The order of the memory chunck that this page belongs to. */
	int order;
	/* Used for ChCore slab allocator. */
	void *slab;
};

struct free_list {
	struct list_head free_list;
	u64 nr_free;
};

/* Disjoint physical memory can be represented by several phys_mem_pool. */
struct phys_mem_pool {
	/*
	 * The start virtual address (for used in kernel) of
	 * this physical memory pool.
	 */
	u64 pool_start_addr;
	u64 pool_mem_size; //整个物理内存池的实际可用空间大小（不包括page元数据结构体）

	/*
	 * This field is only used in ChCore unit test.
	 * The number of (4k) physical pages in this physical memory pool.
	 */
	u64 pool_phys_page_num;

	/*
	 * The start virtual address (for used in kernel) of
	 * the metadata area of this pool.
	 */
	struct page *page_metadata;

	/* The free list of different free-memory-chunk orders. */
	struct free_list free_lists[BUDDY_MAX_ORDER];
};

/* Currently, ChCore only uses one physical memory pool. */
extern struct phys_mem_pool global_mem;

void init_buddy(struct phys_mem_pool *zone, struct page *start_page,
		vaddr_t start_addr, u64 page_num);

struct page *buddy_get_pages(struct phys_mem_pool *, u64 order);
void buddy_free_pages(struct phys_mem_pool *, struct page *page);

void *page_to_virt(struct phys_mem_pool *, struct page *page);
struct page *virt_to_page(struct phys_mem_pool *, void *ptr);
u64 get_free_mem_size_from_buddy(struct phys_mem_pool *);
