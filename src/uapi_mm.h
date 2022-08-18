#ifndef __UAPI_MM__
#define __UAPI_MM__

#include <stdint.h>
#include <string>

/* Initialize the global page size for the memory manager */
void mm_init(); 


/* Instantiate new structure family and accommodate it into the page for families */
void mm_instantiate_new_structure_family(std::string struct_name, uint32_t struct_size);

#define MM_REG_STRUCT(struct_name) \
(mm_instantiate_new_structure_family(#struct_name, sizeof(struct_name))) // '#' converts macro param name to string 


/* Screen out all the registered structure families*/
void mm_print_registered_structure_families();


/* Public function and macro called by the 
 * application for dynamic memory allocation */
void *xcalloc(std::string struct_name, int units);

#define XCALLOC(units, struct_name) \
    xcalloc(#struct_name, units)


/* Public function and macro called by the 
 * application for dynamic memory deallocation */
void xfree(void *app_data);

#define XFREE(data_block_ptr) \
    xfree(data_block_ptr)


/* Iterate all the page families which have registered
 * within the memory manager, and print the memory usage
 * inside the vm pages */
void mm_print_memory_usage();


/* A summary of total number of api occupied blocks (OBC),
 * total number of free data block (FBC) and 
 * total number of meta blocks which have been created (TBC) */
void mm_print_block_usage();

#endif /* __UAPI_MM__ */