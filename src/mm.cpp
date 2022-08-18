#include <iostream>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <memory.h>
#include <assert.h>
#include <iomanip>
#include <sstream>
#include "mm.h"
#include "uapi_mm.h"
#include "gluethread/glthread.h"


size_t SYSTEM_PAGE_SIZE{0};
static PageForStructFamilies *first_vm_page_for_families{nullptr};


/* Initialize the global page size for the memory manager */
void mm_init() {
    /* Get the size of each memory page, 
     * Memory page is 4096 Bytes in my wsl2 */
    SYSTEM_PAGE_SIZE = getpagesize();
}


/* Function to request VM page from kernel, 
 * and returns a pointer to the page we applied */
void *mm_get_new_vm_page_from_kernel(int units) {
    void *vm_page = mmap(
        NULL,
        units * SYSTEM_PAGE_SIZE,
        PROT_READ|PROT_WRITE|PROT_EXEC, /* vm page readable, writable and executable */
        MAP_PRIVATE|MAP_ANONYMOUS, /* private and anonymous mapping */
        0, 0
    );
    /* Terminate the whole program if page allocation fails */
    if (vm_page == MAP_FAILED) {
        std::cerr << "Error: VM Page allocation failed" << std::endl;
        exit(-1);
    }
    /* Initialize the page we get all to zeros */
    memset(vm_page, 0, units * SYSTEM_PAGE_SIZE);
    return vm_page;
}


/* Function to return a page for application back to kernel */
void mm_return_page_for_appln_to_kernel(void *vm_page, int units) {
    if (munmap(vm_page, units * SYSTEM_PAGE_SIZE)) {
        std::cerr << "Error: Could not return VM page to kernel" << std::endl;
        exit(-1);
    }
}


/* Instantiate new structure family and accommodate it into the page for families */
void mm_instantiate_new_structure_family(std::string struct_name, uint32_t struct_size) {
    PageForStructFamilies *new_vm_page_for_families{nullptr};

    /* Not allowed since structure needs continuous page memory */
    if (struct_size > SYSTEM_PAGE_SIZE) { 
        std::cerr << "Error: Structure " << struct_name << " size exceeds system page size" << std::endl;
        exit(-1);
    }
    /* If the page for structure families has not been constructed, construct a new one */
    if (first_vm_page_for_families == nullptr) {
        first_vm_page_for_families = 
            static_cast<PageForStructFamilies*>(mm_get_new_vm_page_from_kernel(1));
        first_vm_page_for_families->structure_family.emplace_back(struct_name, struct_size);
        init_glthread(&first_vm_page_for_families->structure_family.back().free_block_priority_list_head);
        first_vm_page_for_families->next = nullptr;
        return;
    }
    /* If the first page for structure families has been full, construct a new one */
    if (first_vm_page_for_families->structure_family.size() == MAX_FAMILIES_PER_VM_PAGE) {
        new_vm_page_for_families = 
            static_cast<PageForStructFamilies*>(mm_get_new_vm_page_from_kernel(1));
        new_vm_page_for_families->next = first_vm_page_for_families;
        first_vm_page_for_families = new_vm_page_for_families;
    }
    /* Add the structure to the 'hotel' at the very beginning of the program call by 'MM_REG_STRUCT'*/
    first_vm_page_for_families->structure_family.emplace_back(struct_name, struct_size);
    init_glthread(&first_vm_page_for_families->structure_family.back().free_block_priority_list_head);
}


/* Screen out all the registered structure families*/
void mm_print_registered_structure_families() {
    
    PageForStructFamilies* curr_vm_page_for_families{first_vm_page_for_families};
    
    /* Iterate over the vector for structure families */
    while(curr_vm_page_for_families != nullptr) {
        for (const auto &family: curr_vm_page_for_families->structure_family) {
            std::cout << "Page Family: " << family.struct_name 
                      << ", Size = " << family.struct_size << std::endl;
        }
        curr_vm_page_for_families = curr_vm_page_for_families->next;
    }
}


/* Check if a page for application is empty */
vm_bool mm_is_page_for_appln_empty(PageForApplication *page_for_appln) {
    if (page_for_appln->block_meta_data.next_block == nullptr && 
        page_for_appln->block_meta_data.prev_block == nullptr &&
        page_for_appln->block_meta_data.is_free == MM_TRUE) {
            return MM_TRUE;
        }
    return MM_FALSE;
}


/* Initialize lower most Meta block of the page for application manual */
void mm_make_page_for_appln_empty(PageForApplication *page_for_appln) {
    page_for_appln->block_meta_data.next_block = nullptr;
    page_for_appln->block_meta_data.prev_block = nullptr;
    page_for_appln->block_meta_data.is_free = MM_TRUE;
}


/* Allocate a memory page for applications
 * Inside the page is meta block and data block */
PageForApplication *mm_allocate_page_for_application(StructureFamily *structure_family) {
    PageForApplication *page_for_appln = 
        static_cast<PageForApplication*>(mm_get_new_vm_page_from_kernel(1));
    
    /* Initialize lower most Meta block of the page for application manually again */
    mm_make_page_for_appln_empty(page_for_appln);

    page_for_appln->block_meta_data.block_size = 
        mm_max_page_allocatable_memory(1);
    page_for_appln->block_meta_data.offset = 
        offsetof(PageForApplication, block_meta_data);
    init_glthread(&page_for_appln->block_meta_data.priority_thread_glue);
    page_for_appln->prev = nullptr;
    page_for_appln->next = nullptr;

    /* Set a back pointer to page family */
    page_for_appln->structure_family = structure_family;

    /* If it is the first VM data page for a given page family */
    if (structure_family->first_page == nullptr) {
        structure_family->first_page = page_for_appln;
        return page_for_appln;
    }

    /* Insert new VM page to the head of the linked list */
    page_for_appln->next = structure_family->first_page;
    structure_family->first_page->prev = page_for_appln;
    structure_family->first_page = page_for_appln;
    return page_for_appln;
}


/* Delete and free a page for application back to kernel */
void mm_delete_and_free_page_for_application(PageForApplication *page_for_appln) {
    StructureFamily *structure_family = 
        page_for_appln->structure_family;

    /* If the page being deleting is the head of the linked list */
    if (structure_family->first_page == page_for_appln) {
        structure_family->first_page = page_for_appln->next;
        if (page_for_appln->next != nullptr) {
            page_for_appln->next->prev = nullptr;
        }
        page_for_appln->next = nullptr;
        page_for_appln->prev = nullptr;
        mm_return_page_for_appln_to_kernel(static_cast<void*>(page_for_appln), 1);
        return;
    }

    /* Deleting the VM page from the middle or the end of the linked list */
    if (page_for_appln->next != nullptr) {
        page_for_appln->next->prev = page_for_appln->prev;
    }
    page_for_appln->prev->next = page_for_appln->next;
    mm_return_page_for_appln_to_kernel(static_cast<void*>(page_for_appln), 1);
}


/* Local function to compare the block size of two given blocks*/
static int 
mm_free_blocks_comparison_function(void *_block_meta_data1, void *_block_meta_data2) {
    
    BlockMetaData *block_meta_data1 = 
        static_cast<BlockMetaData*>(_block_meta_data1);

    BlockMetaData *block_meta_data2 = 
        static_cast<BlockMetaData*>(_block_meta_data1);

    if (block_meta_data1->block_size > block_meta_data2->block_size) {
        return -1;
    } else if (block_meta_data1->block_size < block_meta_data2->block_size) {
        return 1;
    }
    return 0;
}


/* Add a free data block into the priority queue */
static void 
mm_add_free_block_meta_data_to_free_block_list(
        StructureFamily *structure_family,
        BlockMetaData *free_block) {
    
    assert(free_block->is_free = MM_TRUE);
    glthread_priority_insert(&structure_family->free_block_priority_list_head,
                             &free_block->priority_thread_glue,
                             mm_free_blocks_comparison_function,
                             offsetof(BlockMetaData, priority_thread_glue));
}


/* Function to mark block_meta_data as being Allocated for 'size'
 * bytes of application data.
 * Return true if block allocation succeeds */
static vm_bool
mm_split_free_data_block_for_application(
        StructureFamily *structure_family,
        BlockMetaData *block_meta_data,
        uint32_t size) {
    
    BlockMetaData *next_block_meta_data = nullptr;

    assert(block_meta_data->is_free = MM_TRUE);
    if (block_meta_data->block_size < size) {
        return MM_FALSE;
    }
    
    uint32_t remaining_size = block_meta_data->block_size - size;
    
    block_meta_data->is_free = MM_FALSE;
    block_meta_data->block_size = size;
    remove_glthread(&block_meta_data->priority_thread_glue);

    /*Case 1: No Split*/
    if (remaining_size == 0) {
        return MM_TRUE;
    }

    /*Case 3: Partial Split - Soft Internal Fragmentation*/
    else if (sizeof(BlockMetaData) < remaining_size && 
             remaining_size < sizeof(BlockMetaData) + structure_family->struct_size) {
        next_block_meta_data = next_meta_block_by_size(block_meta_data);
        next_block_meta_data->is_free = MM_TRUE;
        next_block_meta_data->block_size = 
            remaining_size - sizeof(BlockMetaData);
        next_block_meta_data->offset = block_meta_data->offset + 
            sizeof(BlockMetaData) + block_meta_data->block_size;
        mm_add_free_block_meta_data_to_free_block_list(
            structure_family, next_block_meta_data);
        mm_bind_blocks_for_allocation(
            block_meta_data, next_block_meta_data);
    }

    /*Case 3: Partial Split - Hard Internal Fragmentation*/
    else if(remaining_size < sizeof(BlockMetaData)) {
        /*Need do nothing*/
    }

    /*Case 2: Full Split - New Meta Block is Created*/
    else {
        next_block_meta_data = next_meta_block_by_size(block_meta_data);
        next_block_meta_data->is_free = MM_TRUE;
        next_block_meta_data->block_size = 
            remaining_size - sizeof(BlockMetaData);
        next_block_meta_data->offset = block_meta_data->offset + 
            sizeof(BlockMetaData) + block_meta_data->block_size;
        mm_add_free_block_meta_data_to_free_block_list(
            structure_family, next_block_meta_data);
        mm_bind_blocks_for_allocation(
            block_meta_data, next_block_meta_data);
    }
    return MM_TRUE;
}


/* Apply for a new page for application and add the 
 * free block to the priority queue */
static PageForApplication *
mm_family_new_page_add(StructureFamily *structure_family){

    PageForApplication *page_for_appln = 
        mm_allocate_page_for_application(structure_family);

    if(page_for_appln == nullptr)
        return nullptr;

    /* The new page is like one free block, add it to the
     * free block list*/
    mm_add_free_block_meta_data_to_free_block_list(
        structure_family, &page_for_appln->block_meta_data);

    return page_for_appln;
}


/* Called by 'xcalloc' to get the largest data block to 
 * settle down the new data block inside */
static BlockMetaData *mm_allocate_free_data_block(
        StructureFamily *structure_family,
        uint32_t req_size) {

    vm_bool status = MM_FALSE;
    PageForApplication *page_for_appln = nullptr;
    
    BlockMetaData *biggest_block_meta_data = 
        mm_get_biggest_free_block_page_family(structure_family);

    if (biggest_block_meta_data == nullptr || 
            biggest_block_meta_data->block_size < req_size) {
        
        /* Try to add a new page to page family to satisfy the request */
        page_for_appln = mm_family_new_page_add(structure_family);

        /* Allocate the free block from this page new */
        status = mm_split_free_data_block_for_application(structure_family,
                    &page_for_appln->block_meta_data, req_size);
        if (status) {
            return &page_for_appln->block_meta_data;
        }
        return nullptr;
    }

    /*T he biggest block meta data can satisfy the request */
    if (biggest_block_meta_data) {
        status = mm_split_free_data_block_for_application(structure_family,
                    biggest_block_meta_data, req_size);
    }
    if (status) {
        return biggest_block_meta_data;
    }
    return nullptr;
}


/* Iterate over all page for structure families 
 * and find the specific structure registration */
StructureFamily*
mm_lookup_structure_family_by_name(std::string struct_name) {
    
    StructureFamily *structure_family_curr;
    PageForStructFamilies *page_for_families_curr = first_vm_page_for_families;

    /* Iterate over all page for structure families 
     * and find the specific structure registration */
    while(page_for_families_curr != nullptr) {
        for (auto &page_family: page_for_families_curr->structure_family) {
            if (page_family.struct_name == struct_name) {
                structure_family_curr = &page_family;
                return structure_family_curr;
            }
        }
        page_for_families_curr = page_for_families_curr->next;
    }
    return nullptr;
}


/* Public function called by the application for dynamic memory allocation */
void *xcalloc(std::string struct_name, int units) {

    /* Look for structure family by name */
    StructureFamily *structure_family = mm_lookup_structure_family_by_name(struct_name);

    if (structure_family == nullptr) {
        std::cerr << "Error: Structure " << struct_name 
                  << " is not registered in the Memory Manager" << std::endl;
        return nullptr;
    }

    if (units * structure_family->struct_size > MAX_PAGE_ALLOCATABLE_MEMORY) {
        std::cerr << "Error: Memory requested exceeds page size" << std::endl;
        return nullptr;
    }

    /* Find the page which can satisfy the request */
    BlockMetaData *free_block_meta_data = nullptr;
    free_block_meta_data = mm_allocate_free_data_block(
        structure_family, units * structure_family->struct_size);
    
    if (free_block_meta_data) {
        /* Fill in with zero */
        memset((char *)(free_block_meta_data + 1), 0, 
            free_block_meta_data->block_size);
        /* Jump to the data block, instead of meta block */
        return (char *)(free_block_meta_data + 1); 
    }
    return nullptr;
}


/* Get the size of the hard mode free data block */
static int
mm_get_hard_internal_memory_frag_size(
        BlockMetaData *first,
        BlockMetaData *second) {
    
    BlockMetaData *next_block = next_meta_block_by_size(first);
    return (int)((unsigned long)second - (unsigned long)next_block);
}


/* Union two free data blocks */
static void
mm_union_free_blocks(BlockMetaData *first, BlockMetaData *second){

    assert(first->is_free == MM_TRUE &&
        second->is_free == MM_TRUE);

    first->block_size += sizeof(BlockMetaData) +
            second->block_size;

    remove_glthread(&first->priority_thread_glue);
    remove_glthread(&second->priority_thread_glue);
    mm_bind_blocks_for_deallocation(first, second);
}


/* Called by xfree to release the memory used by some application data */
static BlockMetaData*
mm_free_blocks(BlockMetaData *to_be_free_block) {

    BlockMetaData *return_block{nullptr};

    /* Get the page for application where the 'to_be_free_block' lies */
    assert(to_be_free_block->is_free == MM_FALSE);
    PageForApplication *hosting_page = 
        reinterpret_cast<PageForApplication*>(mm_get_page_from_meta_block(to_be_free_block));
    
    /* Get the page family back */
    StructureFamily *structure_family = hosting_page->structure_family;

    /* Free the target meta block, and check different situations */
    return_block = to_be_free_block;
    to_be_free_block->is_free = MM_TRUE;

    BlockMetaData *next_block = next_meta_block(to_be_free_block);

    /* Scenario 1: the data block to be freed is not 
     * the last uppermost block in a application page */
    if (next_block) {
        to_be_free_block->block_size +=
            mm_get_hard_internal_memory_frag_size(to_be_free_block, next_block);
    } 
    /* Scenario 2: Page boundary condition */
    else {
        /* The uppermost top of the page */
        char *end_address_of_vm_page = 
            reinterpret_cast<char *>((char *)hosting_page + SYSTEM_PAGE_SIZE);
        /* The address of the uppermost hard free data block */
        char *end_address_of_free_data_block = 
            reinterpret_cast<char *>(to_be_free_block + 1) + to_be_free_block->block_size;
        /* The size of the uppermost hard free data block */
        uint32_t internal_mem_fragmentation = 
            (int)((unsigned long)end_address_of_vm_page - (unsigned long)end_address_of_free_data_block);
        to_be_free_block->block_size += internal_mem_fragmentation;
    }

    /* Merging process, merge the free blocks above and beneath */
    if (next_block && next_block->is_free == MM_TRUE) {
        /* Union two free blocks */
        mm_union_free_blocks(to_be_free_block, next_block);
        return_block = to_be_free_block;
    }
    BlockMetaData *prev_block = prev_meta_block(to_be_free_block);
    if (prev_block && prev_block->is_free == MM_TRUE) {
        mm_union_free_blocks(prev_block, to_be_free_block);
        return_block = prev_block;
    }

    /* If the page for application is empty, release the page back to kernal */
    if (mm_is_page_for_appln_empty(hosting_page)) {
        mm_delete_and_free_page_for_application(hosting_page);
        return nullptr;
    }

    /* Add the big empty data block to the priority queue */
    mm_add_free_block_meta_data_to_free_block_list(
        hosting_page->structure_family, return_block);

    return return_block;
}


/* The API for application call to free application data block */
void xfree(void *app_data) {

    /*Get the guardian meta block of current data block by subtracting the size of Meta Block*/
    BlockMetaData *block_meta_data = 
        reinterpret_cast<BlockMetaData*>((char *)app_data - sizeof(BlockMetaData));
    
    /*Assert we get the right thing, and free the data block*/
    if(block_meta_data->is_free == MM_TRUE){
        std::cerr << "Error: Double free detected" << std::endl;
        exit(-1);
    }
    mm_free_blocks(block_meta_data);
}


/* Iterate all the page families which have registered
 * within the memory manager, and print the memory usage
 * inside the vm pages */
void mm_print_memory_usage() {
    
    PageForApplication *page_for_appln_curr{nullptr};
    PageForStructFamilies *vm_page_for_families_curr{nullptr};
    BlockMetaData *block_meta_data_curr{nullptr};

    uint32_t page_count{0};
    uint32_t block_count{0};
    uint32_t block_size{0};
    uint32_t offset{0};
    uint32_t total_pages{0};
    uint32_t total_memory{0};
    
    std::string prev_block_addr;
    std::string curr_block_addr;
    std::string next_block_addr;
    std::string block_status;

    const uint32_t table_indent     {22};
    const uint32_t block_num_len    {4};
    const uint32_t block_size_len   {6};
    const uint32_t offset_len       {6};
    const uint32_t pred_addr_len    {16};
    const uint32_t next_addr_len    {16};

    std::cout << "\nPage Size = " << SYSTEM_PAGE_SIZE << " Bytes" << std::endl;

    vm_page_for_families_curr = first_vm_page_for_families;

    /* Iterate over all the page for structure families */
    while(vm_page_for_families_curr) {
        
        /* For each family, do something */
        for(const auto &structure_family: 
                vm_page_for_families_curr->structure_family) {
            
            page_count = 0;
            std::cout << "\033[32mStructure Family: " << structure_family.struct_name
                      << ", struct size = " << structure_family.struct_size << "\033[0m\n";

            page_for_appln_curr = structure_family.first_page;
            
            /* Iterate over all the page for application derive from the family */
            while(page_for_appln_curr) {
                
                page_count++;
                total_pages++;
                block_count = 0;

                std::string prev_appln_page_addr = 
                    get_format_pointer_address(page_for_appln_curr->prev);
                std::string next_appln_page_addr =
                    get_format_pointer_address(page_for_appln_curr->next);
                std::string local_appln_page_addr = 
                    get_format_pointer_address(page_for_appln_curr);

                std::cout << std::setfill(' ') << std::setw(18) << ' '
                          << "prev = " << prev_appln_page_addr
                          << ", local = " << local_appln_page_addr
                          << ", next = " << next_appln_page_addr << std::endl;

                std::cout << std::setfill(' ') << std::setw(18) << ' '
                          << "structure family = " << structure_family.struct_name 
                          << ", count = " << page_count
                          << std::endl;

                block_meta_data_curr = 
                    &page_for_appln_curr->block_meta_data;
                
                /* Iterate over all data block inside the page for appln */
                while(block_meta_data_curr){

                    if(block_meta_data_curr->is_free == MM_FALSE){
                        assert(IS_GLTHREAD_LIST_EMPTY(
                            &block_meta_data_curr->priority_thread_glue));
                    }

                    if(block_meta_data_curr->is_free == MM_TRUE){
                        assert(!IS_GLTHREAD_LIST_EMPTY(
                            &block_meta_data_curr->priority_thread_glue));
                    }
                    
                    prev_block_addr = get_format_pointer_address(
                        block_meta_data_curr->prev_block);
                    curr_block_addr = get_format_pointer_address(
                        block_meta_data_curr);
                    next_block_addr = get_format_pointer_address(
                        block_meta_data_curr->next_block);

                    block_count++;
                    offset = block_meta_data_curr->offset;
                    block_size = block_meta_data_curr->block_size;
                    block_status = (block_meta_data_curr->is_free == MM_TRUE) ? 
                        "\033[32mFREEBLOCK\033[0m  " : "ALLOCATED  ";

                    std::cout << std::setfill(' ') << std::setw(table_indent) << ' '
                              << curr_block_addr << "  Block " 
                              << std::left << std::setw(block_num_len) << block_count
                              << std::left << block_status
                              << std::left << "block_size = " << std::setw(block_size_len) << block_size
                              << std::left << "offset = " << std::setw(offset_len) << offset
                              << std::left << "prev = " << std::setw(pred_addr_len) << prev_block_addr
                              << std::left << "next = " << std::setw(next_addr_len) << next_block_addr
                              << std::endl;

                    block_meta_data_curr = 
                        block_meta_data_curr->next_block;
                }
                std::cout << std::endl;
                page_for_appln_curr = 
                    page_for_appln_curr->next;
            }
        }
        vm_page_for_families_curr = 
            vm_page_for_families_curr->next;
    }

    total_memory = total_pages * SYSTEM_PAGE_SIZE;
    std::cout << "\033[35m# Of VM Pages in Use : " << total_pages 
              << " (" << total_memory << " Bytes)\033[0m\n";
    std::cout << "Total Memory being used by Memory Manager = "
              << total_memory << " Bytes" << std::endl;

}


/* A summary of total number of api occupied blocks (OBC),
 * total number of free data blocks (FBC) and 
 * total number of meta blocks which have been created (TBC) */
void mm_print_block_usage() {

    PageForApplication *page_for_appln_curr{nullptr};
    PageForStructFamilies *vm_page_for_families_curr{nullptr};
    BlockMetaData *block_meta_data_curr{nullptr};

    uint32_t application_memory_usage{0};
    uint32_t total_block_count{0}, free_block_count{0}, occupied_block_count{0};
    
    const uint32_t name_length              {20};
    const uint32_t total_count_length       {12};
    const uint32_t free_block_length        {12};
    const uint32_t occup_block_length       {12};
    const uint32_t appln_usage_length       {12};

    vm_page_for_families_curr = first_vm_page_for_families;

    /* Iterate over all the page for structure families */
    while(vm_page_for_families_curr) {
        
        /* For each family, do something */
        for(const auto &structure_family: 
                vm_page_for_families_curr->structure_family) {
            
            total_block_count = 0;
            free_block_count = 0;
            occupied_block_count = 0;
            application_memory_usage = 0;

            page_for_appln_curr = structure_family.first_page;
            
            /* Iterate over all the page for application derive from the family */
            while(page_for_appln_curr) {
                
                block_meta_data_curr = 
                    &page_for_appln_curr->block_meta_data;
                
                /* Iterate over all data block inside the page for appln */
                while(block_meta_data_curr){
                    
                    total_block_count++;

                    if(block_meta_data_curr->is_free == MM_FALSE){
                        assert(IS_GLTHREAD_LIST_EMPTY(
                            &block_meta_data_curr->priority_thread_glue));
                    }

                    if(block_meta_data_curr->is_free == MM_TRUE){
                        assert(!IS_GLTHREAD_LIST_EMPTY(
                            &block_meta_data_curr->priority_thread_glue));
                    }

                    if(block_meta_data_curr->is_free == MM_TRUE){
                        free_block_count++;
                    }
                    else{
                        /* Do not include the guardian meta block of the top empty one */
                        application_memory_usage += 
                            block_meta_data_curr->block_size + sizeof(BlockMetaData);
                        occupied_block_count++; /* One large block is also A BLOCK, not several units of blocks */
                    }
                    block_meta_data_curr = 
                        block_meta_data_curr->next_block;
                }
                page_for_appln_curr = 
                    page_for_appln_curr->next;
            }
            /* Statistic information screen out */
            std::cout << std::setw(name_length) << std::left << structure_family.struct_name
                      << std::left << "TBC: " << std::setw(total_count_length) << total_block_count
                      << std::left << "FBC: " << std::setw(free_block_length) << free_block_count
                      << std::left << "OBC: " << std::setw(occup_block_length) << occupied_block_count
                      << std::left << "AppMemUsage: " << std::setw(appln_usage_length) << application_memory_usage
                      << std::endl;
        }
        vm_page_for_families_curr = 
            vm_page_for_families_curr->next;
    }
}