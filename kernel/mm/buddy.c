#include <common/util.h>
#include <common/macro.h>
#include <common/kprint.h>

#include "buddy.h"


/*
 * The layout of a phys_mem_pool:
 * | page_metadata are (an array of struct page) | alignment pad | usable memory |
 * 将layout改成竖向结构可能更好理解
 * 
 * ————————————————————————————————————————————
 * page_metadata are (an array of struct page)(由许多个page的元数据结构体组成)
 * ————————————————————————————————————————————
 * alignment pad（为了对齐而填充的空白）
 * ————————————————————————————————————————————
 * usable memore
 * ————————————————————————————————————————————
 * 参照网上的说法，应该有个位图用来记录伙伴物理块分配与否，但是这里的数据结构没有提供，
 * 然而只需要计算出伙伴的地址，就能找到其page metadata，就可以知道其分配与否，这个操作可以取代位图（虽然没这么快，但是更新也没那么繁琐）。
 * （错误：然后还是要遍历free_list来找这个伙伴- -）
 * 在实现mer_page时才明白page->node的真正作用，当我知道buddy的地址时，就知道了其page，也就知道了其在free_list的位置（由node指定），
 * 由于free_list是个双向链表，因此可以快速删除，用这种巧妙地方式实现了类似c++里map的功能。
 * 
 * The usable memory: [pool_start_addr, pool_start_addr + pool_mem_size).
 * 
 * 1、初始化各种数据结构，包括物理池和free list
 * 2、将page的元数据清零，此时每个page阶数都是0
 * 3、调用buddy_free_pages函数，相当于对每个页进行一个释放操作，由于释放操作包含合并操作，因此运行后所有物理页（此时都是空闲的）都会合在一起便臣一个大的chunk。
 */
void init_buddy(struct phys_mem_pool *pool, struct page *start_page,
		vaddr_t start_addr, u64 page_num)
{
	int order;
	int page_idx;
	struct page *page;

	/* Init the physical memory pool. */
	pool->pool_start_addr = start_addr;
	pool->page_metadata = start_page;
	pool->pool_mem_size = page_num * BUDDY_PAGE_SIZE;
	/* This field is for unit test only. */
	pool->pool_phys_page_num = page_num;

	/* Init the free lists */
	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		pool->free_lists[order].nr_free = 0;
		init_list_head(&(pool->free_lists[order].free_list));
	}

	/* Clear the page_metadata area. */
	memset((char *)start_page, 0, page_num * sizeof(struct page));

	/* Init the page_metadata area. */
	for (page_idx = 0; page_idx < page_num; ++page_idx) {
		page = start_page + page_idx;
		page->allocated = 1;
		page->order = 0;
		//此时page->node的值都是0
		//具体来说page结构体前16个字节存放的是node->prev和node->next的值，但此时初始化为0。
	}

	/* Put each physical memory page into the free lists. */
	for (page_idx = 0; page_idx < page_num; ++page_idx) {
		page = start_page + page_idx;
		buddy_free_pages(pool, page);
	}
}

//判断地址是否合法
// bool check_addr(struct phys_mem_pool *pool, u64 addr){
// 	if ((addr < pool->pool_start_addr) ||
// 	    (addr >= (pool->pool_start_addr +
// 				  pool->pool_mem_size))) {
// 		return false;
// 	}
// 	return true;
// }

//获取这个page所属的chunk的伙伴chunk。
//page-某个物理块（大小为4096），chunk-多个连续的物理块合在一起的大内存。
//假设page1属于chunk1，page2属于chunk2，那么输入page1的地址，这个函数将会返回chunk2的起始地址，反之亦然。
static struct page *get_buddy_chunk(struct phys_mem_pool *pool,
				    struct page *chunk)
{
	u64 chunk_addr;
	u64 buddy_chunk_addr;
	int order;
	/* Get the address of the chunk. */
	chunk_addr = (u64) page_to_virt(pool, chunk);
	order = chunk->order;
	// printk("get buddy chunk, this order:%d\n", order);
	/*
	 * Calculate the address of the buddy chunk according to the address
	 * relationship between buddies.
	 * 12表示的是一个物理块的大小
	 * 1UL << (order + BUDDY_PAGE_SIZE_ORDER) 表示的是这个chunk的总大小。
	 * 假设一个chunk的order是0，那么1UL << (order + BUDDY_PAGE_SIZE_ORDER) 移位后就是2的12次方，也就是第13位为1。
	 * 此时第一个chunk的第13是0，第二个chunk的第13位为1。
	 * 第一个chunk和1异或结果是1，第二个chunk和1异或结果是0，恰好是伙伴chunk的起始地址。
	 * 为什么是异或？当前page所属的chunk可能是前一个伙伴，也可能是后一个伙伴，所以需要异或来找到另一个伙伴。
	 * 注意：当order大于0时，此时该chunk内所有物理块连在一起，并且只保留第一个物理块的起始地址作为这个chunk的起始地址。
	 * 也就是说，假设order为3，那么相邻两个伙伴的起始地址的前四位（后面十二位都是0不写了）只可能是0000和1000。不会出现0001等其他的情况。
	 */

#define BUDDY_PAGE_SIZE_ORDER (12)
	buddy_chunk_addr = chunk_addr ^
	    (1UL << (order + BUDDY_PAGE_SIZE_ORDER));

	/* Check whether the buddy_chunk_addr belongs to pool. */
	// if(!check_addr(pool, buddy_chunk_addr))
	// 	return NULL;
	if ((buddy_chunk_addr < pool->pool_start_addr) ||
	    (buddy_chunk_addr >= (pool->pool_start_addr +
				  pool->pool_mem_size))) {
		//由于地址起始的问题，某些块的buddy会超过界限
		//但这是没所谓的，因为这是物理地址，而内核实际使用的是虚拟地址，虚拟地址保证从0开始即可，具体映射到哪个物理地址无所谓
		printk("Buddy addr exceed! start page addr:%lx end page addr:%lx buddy addr:%lx page addr:%lx order:%d\n", pool->pool_start_addr, (pool->pool_start_addr +
				  pool->pool_mem_size), buddy_chunk_addr, chunk_addr, order);
		return NULL;
	}

	return virt_to_page(pool, (void *)buddy_chunk_addr);
}


/*  
从物理池第order的free_list处取出第一个空闲块
但是取出块后不一定是分配，可能是进一步切割，所以page->allocated需要函数返回后自己调整
*/
static struct page *get_page_from_freelist(struct phys_mem_pool *pool, u64 order){
	struct page* page = NULL;
	if(order < 0 || order >= BUDDY_MAX_ORDER || pool->free_lists[order].nr_free == 0)
		return page;
	page = list_entry(pool->free_lists[order].free_list.next, struct page, node);
	list_del(&page->node);
	--pool->free_lists[order].nr_free;
	return page;
}



/*
 * split_page: split the memory block into two smaller sub-block, whose order
 * is half of the origin page.
 * pool @ physical memory structure reserved in the kernel
 * order @ order for origin page block
 * page @ splitted page
 * 
 * Hints: don't forget to substract the free page number for the corresponding free_list.
 * you can invoke split_page recursively until the given page can not be splitted into two
 * smaller sub-pages.
 */
static struct page *split_page(struct phys_mem_pool *pool, u64 order,
			       struct page *page)
{
	// <lab2>
	struct page *splited_page = NULL, *page2;
	// u64 addr1, addr2;
	if(order <= 0 || order >= BUDDY_MAX_ORDER)
		return splited_page;

	//先计算划分后的地址，再找到其对应的page metadata
	// #define BUDDY_PAGE_SIZE_ORDER (12)
	// addr1 = (u64) page_to_virt(pool, page);
	// addr2 = addr1 | (1UL << (order - 1 + BUDDY_PAGE_SIZE_ORDER)); 
	
	//将要返回的块的分配位设置为1，防止另一个块释放时合并这个块
	splited_page = page;
	splited_page->order = order - 1;
	splited_page->allocated = 1;
	// printk("find another chunk order:%d\n", splited_page->order);
	//释放另一个块
	// page2 = virt_to_page(pool, (void *) addr2);

	page2 = get_buddy_chunk(pool, splited_page);
	page2->order = order - 1;
	// printk("put another chunk into free list, order:%d\n", page2->order);
	buddy_free_pages(pool, page2);

	return splited_page;
	// </lab2>
}

/* 
从目标chunk中切出实际需要的内存块
@pool	物理内存池
@origin_order	原来内存块的order
@need_order		实际需要的内存块order
由调用函数来保证origin_order和need_order的合法性
 */
static struct page *split_need_page(struct phys_mem_pool *pool, u64 origin_order, u64 need_order){
	struct page *page = NULL;
	page = get_page_from_freelist(pool, origin_order);
	// printk("begin split need page %d %d\n", origin_order,need_order);
	if(!page)
		return NULL;
	page->allocated = 1;
	while(origin_order != need_order){
		// printk("%d %d\n", origin_order, need_order);
		page = split_page(pool, origin_order, page);
		origin_order--;
	}
	return page;
}

/*
 * buddy_get_pages: get free page from buddy system.
 * pool @ physical memory structure reserved in the kernel
 * order @ get the (1<<order) continous pages from the buddy system
 * 
 * Hints: Find the corresonding free_list which can allocate 1<<order
 * continuous pages and don't forget to split the list node after allocation   
 */
struct page *buddy_get_pages(struct phys_mem_pool *pool, u64 order)
{
	// <lab2>
	// kinfo("begin get pages\n");
	struct page *page = NULL;
	int i;
	if(order >= BUDDY_MAX_ORDER)
		return page;
	
	for(i = order; i < BUDDY_MAX_ORDER; i++){
		if(pool->free_lists[i].nr_free > 0)
			break;
	}
	// printk("need order: %d have order: %d\n", order, i);

	if(i == BUDDY_MAX_ORDER)
		return page;

	page = split_need_page(pool, i, order);


	return page;
	// </lab2>
}

/*
 * merge_page: merge the given page with the buddy page
 * pool @ physical memory structure reserved in the kernel
 * page @ merged page (attempted)
 * 
 * Hints: you can invoke the merge_page recursively until
 * there is not corresponding buddy page. get_buddy_chunk
 * is helpful in this function.
 * 一、获取伙伴chunk地址
 * 二、判断其是否已经分配
 * 三、若没有则：1.将伙伴从原来的链表删掉；2.合并；3.将合并后的块插入更上一级的free_list
 * 问题：合并后修改哪些page的order？全部都修改？假设此时一个chunk由16个page组成，其合并后，16个page的metadata都要修改吗？还是只需要修改第一个。
 * 引发问题的关键：当一个chunk由四个块组成时，会不会出现使用非第一个块来调用函数的情况。比如get_buddy_chunk，如果使用第一个块可以得到正确结果，使用非第一个块就会得到错误结果。
 * 思考：假如一直只从free_list里取空闲块，那么只修改第一个page的metadata就可以了，因为之后一直使用的都是第一个块的地址。
 * 况且从get_buddy_chunk可以看出，假设一个chunk的order是1，即这个chunk由两个物理块组成，那么get_buddy_chunk(第一个块地址)可以得到正确的伙伴块地址（也就是伙伴块第一个子块的地址），
 * 但是get_buudy_chunk(第二个块地址)不能得到正确的的伙伴块地址，此时得到的是伙伴块的第二个子块的地址。（做lab2一时的假设）
 * 
 * 如果后续实验会出现上面的情况，那么分配和切割时修改所有字块的order即可。
 * 
 */
static struct page *merge_page(struct phys_mem_pool *pool, struct page *page)
{
	// <lab2>

	struct page *merged_page = NULL, *buddy;
	// printk("In merge page get buddy chunk order:%d\n", page->order);
	buddy = get_buddy_chunk(pool, page);
	//取buddy操作可能会失败
	//当buddy越界后就将自己放进free_list里
	//当page的order到达上限时，应该直接放入链表中

	//todo：为什么在buddy没分配时会出现buddy->order!=page->order的情况？
	//原因：假设某个块的地址是0000
	//那么这个块是 0001 order=0的buddy，也是0010 order=1的buddy，也是0100 order=2的buddy
	//此时，如果0000只是order为0的未分配的块，而此时0100寻找buddy会发现其buddy（也就是0000）是未分配的！
	//但这时候一个order为0，一个order为2，显然不匹配。此时会导致内存泄漏。
	if(!buddy || page->order == BUDDY_MAX_ORDER-1 || buddy->allocated || buddy->order != page->order){
		// if(!buddy)
		// 	printk("buddy is none! page addr:%lx\n", page);
		list_add(&(page->node), &(pool->free_lists[page->order].free_list));
		pool->free_lists[page->order].nr_free++;
		merged_page = page;
	}else{
		list_del(&buddy->node);
		pool->free_lists[buddy->order].nr_free--;
		buddy->order++;
		page->order++;
		if((u64) page > (u64) buddy){
			merged_page = buddy;
		}else{
			merged_page = page;
		}
		merge_page(pool, merged_page);
	}
	
	return merged_page;
	// </lab2>
}

/*
 * buddy_free_pages: give back the pages to buddy system
 * pool @ physical memory structure reserved in the kernel
 * page @ free page structure
 * 
 * Hints: you can invoke merge_page.
 */
void buddy_free_pages(struct phys_mem_pool *pool, struct page *page)
{
	// <lab2>
	// printk("buddy free page order:%d\n", page->order);
	page->allocated = 0;
	merge_page(pool, page);
	// </lab2>
}

void *page_to_virt(struct phys_mem_pool *pool, struct page *page)
{
	u64 addr;

	/* page_idx * BUDDY_PAGE_SIZE + start_addr */
	addr = (page - pool->page_metadata) * BUDDY_PAGE_SIZE + pool->pool_start_addr;
	return (void *)addr;
}

struct page *virt_to_page(struct phys_mem_pool *pool, void *addr)
{
	struct page *page;

	page = pool->page_metadata +
	    (((u64) addr - pool->pool_start_addr) / BUDDY_PAGE_SIZE);
	return page;
}

u64 get_free_mem_size_from_buddy(struct phys_mem_pool * pool)
{
	int order;
	struct free_list *list;
	u64 current_order_size;
	u64 total_size = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; order++) {
		/* 2^order * 4K */
		current_order_size = BUDDY_PAGE_SIZE * (1 << order);
		list = pool->free_lists + order;
		total_size += list->nr_free * current_order_size;

		/* debug : print info about current order */
		kdebug("buddy memory chunk order: %d, size: 0x%lx, num: %d\n",
		       order, current_order_size, list->nr_free);
	}
	return total_size;
}
