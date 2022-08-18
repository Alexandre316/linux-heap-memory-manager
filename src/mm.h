#ifndef __MM_H__
#define __MM_H__
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>
#include "gluethread/glthread.h"


extern size_t SYSTEM_PAGE_SIZE;
enum vm_bool: bool {MM_FALSE = false, MM_TRUE = true};
struct PageForApplication;


/* A data structure family struct,
 * a family must check in at the very beginning */
struct StructureFamily {
    std::string struct_name;
    uint32_t struct_size{};
    PageForApplication *first_page{nullptr};
    glthread_t free_block_priority_list_head;
    StructureFamily(std::string name_val = "None", uint32_t size_val = 0);
};
/* Constructor of StructureFamily */
StructureFamily::StructureFamily(std::string name_val, uint32_t size_val)
: struct_name{name_val}, struct_size{size_val} {  
}


/* A 'hotel' page for structure families to check in */
struct PageForStructFamilies {
    PageForStructFamilies *next{nullptr};
    std::vector<StructureFamily> structure_family;
};


/* Meta Block - The guardian of Data Block
 * Data Block is 'block_size' above the Meta Block */
struct BlockMetaData {
    vm_bool is_free{MM_TRUE};
    uint32_t block_size{};
    uint32_t offset{}; /* offset from the start of the page to self location */
    glthread_t priority_thread_glue;
    BlockMetaData *prev_block{nullptr};
    BlockMetaData *next_block{nullptr};
};


/* Page for application to use
 * A double linked list with a pointer to the structure family it derive from */
struct PageForApplication {
    PageForApplication *next{nullptr};
    PageForApplication *prev{nullptr};
    StructureFamily *structure_family{nullptr}; 
    BlockMetaData block_meta_data; /* first meta block right at the bottom */
    char page_memory[0];
};


/* From a specific meta block get the page ptr by
 * subtracting the block's offset */
inline void*
mm_get_page_from_meta_block(BlockMetaData *block_meta_data_ptr) {
    return ((void *) ((char *)block_meta_data_ptr - block_meta_data_ptr->offset));
}


/* Short cut of getting the next data block */
inline BlockMetaData* 
next_meta_block(BlockMetaData *block_meta_data_ptr) {
    return block_meta_data_ptr->next_block;
}


/* Get the next meta block by transforming the current
 * meta pointer to unit pointer and incrementing the 
 * size of the data block it is guarding */
inline BlockMetaData*
next_meta_block_by_size(BlockMetaData *block_meta_data_ptr) {
    return (BlockMetaData *)((char * )(block_meta_data_ptr + 1) + 
        block_meta_data_ptr->block_size);
}


/* Short cut of getting the previous data block */
inline BlockMetaData* 
prev_meta_block(BlockMetaData *block_meta_data_ptr) {
    return block_meta_data_ptr->prev_block;
}


/* Bind two free data block for allocation usage */
inline void mm_bind_blocks_for_allocation(
        BlockMetaData *allocated_meta_block,
        BlockMetaData *free_meta_block) {

    free_meta_block->prev_block = allocated_meta_block;
    free_meta_block->next_block = allocated_meta_block->next_block;
    allocated_meta_block->next_block = free_meta_block;
    if (free_meta_block->next_block) {
        free_meta_block->next_block->prev_block = free_meta_block;
    }
}


/* Bind two free data block for deallocation usage */
inline void mm_bind_blocks_for_deallocation(
        BlockMetaData *freed_meta_block_down,
        BlockMetaData *freed_meta_block_top) {
    
    freed_meta_block_down->next_block = freed_meta_block_top->next_block;
    if (freed_meta_block_top->next_block) {
        freed_meta_block_top->next_block->prev_block = freed_meta_block_down;
    }
}


/* Return the max bytes available to applications 
 * ('page memory' is on the highest address) */
inline uint32_t mm_max_page_allocatable_memory(int units) {
    return (uint32_t) ((SYSTEM_PAGE_SIZE * units) - 
        offsetof(PageForApplication, page_memory));
}

const uint32_t MAX_PAGE_ALLOCATABLE_MEMORY = 
    mm_max_page_allocatable_memory(1);


/* Get the pointer of the meta data block of a 
 * glue thread that lies in by subtracting the offset */
inline BlockMetaData*
glthread_to_block_meta_data(glthread_t *glthreadptr) {
    return (BlockMetaData *)((char *)(glthreadptr) - 
        (char *)&(((BlockMetaData *)0)->priority_thread_glue));
}


/* Get the biggest size data block for worst fit use */
inline BlockMetaData*
mm_get_biggest_free_block_page_family(
        StructureFamily *structure_family) {

    glthread_t *biggest_free_block_glue = 
        structure_family->free_block_priority_list_head.right;
    
    if(biggest_free_block_glue)
        return glthread_to_block_meta_data(biggest_free_block_glue);

    return nullptr;
}


/* Template function to get the format address of a pointer that it points to */
template <typename T>
inline std::string get_format_pointer_address(T ptr) {
    
    std::stringstream ss_obj;
    std::string output_obj;
    
    ss_obj << ptr;
    ss_obj >> output_obj;
    
    output_obj = (output_obj == "0") ? "(nil)" : output_obj;
    return output_obj;
}


/* Function declaration */
/* Allocate virtual memory page for applications */
PageForApplication *mm_allocate_page_for_application(StructureFamily *structure_family);


/* Function declaration */
/* Delete and free page for application to kernel */
void mm_delete_and_free_page_for_application(PageForApplication *page_for_appln);


const uint32_t MAX_FAMILIES_PER_VM_PAGE = 
    (SYSTEM_PAGE_SIZE - sizeof(PageForStructFamilies*)) / sizeof(StructureFamily);


#endif