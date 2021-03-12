/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (2)
 */

/*
 * STUDENT NUMBER: s2002579
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	17

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];
		
		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;
		
		// Return the insert point (i.e. slot)
		return slot;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure the input source_order is correct.
		if (source_order >= MAX_ORDER) {
			return NULL;
		}

		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		PageDescriptor *first_block = *block_pointer;
		
		//Remove the block from starting page.
		remove_block(first_block, source_order);
		
		//Insert two blocks 
		insert_block(first_block, source_order - 1);
		PageDescriptor *next_block = buddy_of(first_block, source_order - 1);
		insert_block(next_block, source_order - 1);
		
		return first_block;
	}
	
	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		if (source_order >= MAX_ORDER)
			return NULL;

		assert(*block_pointer);
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		// Record the start block and its buddy.
		PageDescriptor *start = *block_pointer;
		PageDescriptor *buddy = buddy_of(start, source_order);

		// Remove two blocks.
		remove_block(start, source_order);
		remove_block(buddy, source_order);
		PageDescriptor **left;

		// Merge the block and make sure the pointer is always point to the left block.
		if(buddy > start){
			left = insert_block(start, source_order + 1);
		}
		else{
			left = insert_block(buddy, source_order + 1);
			left = &buddy;
		}
		return left;
	}
	
public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}
	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		
		if(order >= MAX_ORDER){
			return NULL;
		}

		int i = order;
		PageDescriptor **to_split;

		// Find whether this order is suitable for the contiguous pages.
		while(_free_areas[i] == NULL){
			i++;
			if(i >= MAX_ORDER){
				return NULL;
			}
		}
		// If the block is not fit my contiguous pages, split the block until the size is same as pages.
		for(i;i > order;i--){
			to_split = &_free_areas[i];	
			split_block(to_split,i);
		}

		// Return the first block one the order.
		PageDescriptor *first_pointer= _free_areas[order]; 

		// Allocate contiguous pages.
		remove_block(first_pointer,order);
		return first_pointer;
	}
	
	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));

		if (order >= MAX_ORDER)
			return;
		
		if (!is_correct_alignment_for_order(pgd, order)) 
			return;
		
		int now_order = 0;

		PageDescriptor *actual_buddy = buddy_of(pgd, order);
		PageDescriptor **middle_block = insert_block(pgd, order);
		PageDescriptor *viewed_buddy = _free_areas[now_order];

		while(now_order < MAX_ORDER - 1){
			// If there are no available buddy on this order.
			if(viewed_buddy == NULL){
				// Perpare for seaching whether pgd is on upper order.
				now_order++;
				viewed_buddy = _free_areas[now_order];
				// Go into next loop.
				continue;
			}
			// If find buddy in this order.
			else if(viewed_buddy == actual_buddy){
				// Free continguous pages.
				middle_block = merge_block(middle_block, now_order);
				now_order++;
				viewed_buddy = _free_areas[now_order];
				// Check whther the new block has any buddy or not.
				actual_buddy = buddy_of(*middle_block, now_order);			
			}
			// Make buddy point to next block, if available block can be found on this order and does not have buddy of a buddy. 
			else{
				viewed_buddy = viewed_buddy->next_free;
			}
		}	
	}

	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd)
	{
		assert(pgd);

		for(int order_num = 0; order_num <= MAX_ORDER; order_num++){
			// Find the first block of an order.
			PageDescriptor *first_block = _free_areas[order_num];
			while (first_block != nullptr){	
				// Compare and check whether pgd is in the block or not. If yes, break loop. If not, point to next block.			
				if((pgd >= first_block) && (pgd <= first_block + pages_per_block(order_num)))
					break;				
				first_block = first_block->next_free;
			}
			
			// if the order has no available blocks, go in to loop again to search for next order.
			if(first_block == nullptr)
				continue;
			
			// If find pgd in the block, continuously split the block until order equals to 0.
			else{
				while(order_num != 0){	
					// Split the block until order is 0.	
					split_block(&first_block, order_num);		
					order_num--;
					// Find the buddy of the block.
					PageDescriptor *buddy = buddy_of(first_block, order_num);
					// Make sure pointer always point to the left block.
					if(pgd >= buddy)
						first_block = buddy;
				}	
				// Reserve the specific page.
				remove_block(pgd,0);
			}
			return true;
		 }
		 // Fail to find any available block, return false.
		return false;
	}
	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);

		// Calculate how many blocks when initialising.
		int block_size = pages_per_block(MAX_ORDER - 1);
		int num = nr_page_descriptors/block_size;
		int i = 1;

		// Insert these numbers of blocks into free list.
		PageDescriptor *start_pointer = page_descriptors;
		_free_areas[MAX_ORDER - 1] = start_pointer;
		for(i; i < num; i++){
			start_pointer->next_free = start_pointer + pages_per_block(MAX_ORDER - 1);
			start_pointer = start_pointer->next_free;
		}

		// Check whether initialising is success.
		if(i == num)
			return true;
		else
			return false;
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
						
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

	
private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
