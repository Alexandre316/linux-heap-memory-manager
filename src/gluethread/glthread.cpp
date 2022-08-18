#include "glthread.h"
#include <stdlib.h>

/*Initialize a null glue node*/
void init_glthread(glthread_t *glthread){
    glthread->left = nullptr;
    glthread->right = nullptr;
}

/*Insert a new node right on the rhs of the current node*/
void glthread_add_next(
    glthread_t *curr_glthread, glthread_t *new_glthread){

    if(curr_glthread->right == nullptr){
        curr_glthread->right = new_glthread;
        new_glthread->left = curr_glthread;
        return;
    }

    glthread_t *temp = curr_glthread->right;
    curr_glthread->right = new_glthread;
    new_glthread->left = curr_glthread;
    new_glthread->right = temp;
    temp->left = new_glthread;
}

/*Insert a new node right on the lhs of the current node*/
void glthread_add_before(
    glthread_t *curr_glthread, glthread_t *new_glthread){
    
    if(curr_glthread->left == nullptr){
        new_glthread->left = nullptr;
        new_glthread->right = curr_glthread;
        curr_glthread->left = new_glthread;
        return;
    }
    
    glthread_t *temp = curr_glthread->left;
    temp->right = new_glthread;
    new_glthread->left = temp;
    new_glthread->right = curr_glthread;
    curr_glthread->left = new_glthread;
}

/*Remove a node from a priority linked list*/
void remove_glthread(glthread_t *curr_glthread){
    
    if(curr_glthread->left == nullptr){
        if(curr_glthread->right){
            curr_glthread->right->left = nullptr;
            curr_glthread->right = nullptr;
            return;
        }
        return;
    }

    if(curr_glthread->right == nullptr){
        curr_glthread->left->right = nullptr;
        curr_glthread->left = nullptr;
        return;
    }

    curr_glthread->left->right = curr_glthread->right;
    curr_glthread->right->left = curr_glthread->left;
    curr_glthread->left = nullptr;
    curr_glthread->right = nullptr;
}

/*Delete the priority linked list wholely*/
void delete_glthread_list(glthread_t *base_glthread){

    glthread_t *glthreadptr = nullptr;
               
    ITERATE_GLTHREAD_BEGIN(base_glthread, glthreadptr){
        remove_glthread(glthreadptr);
    } ITERATE_GLTHREAD_END(base_glthread, glthreadptr);
}

/*Add a node to the tail of a priority linked list*/
void
glthread_add_last(glthread_t *base_glthread, glthread_t *new_glthread){

    glthread_t *glthreadptr = nullptr,
               *prevglthreadptr = nullptr;
    
    ITERATE_GLTHREAD_BEGIN(base_glthread, glthreadptr){
        prevglthreadptr = glthreadptr;
    } ITERATE_GLTHREAD_END(base_glthread, glthreadptr);
  
    if(prevglthreadptr) 
        glthread_add_next(prevglthreadptr, new_glthread); 
    else
        glthread_add_next(base_glthread, new_glthread);
}

/*Count the number of nodes in a priority linked list*/
unsigned int
get_glthread_list_count(glthread_t *base_glthread){

    unsigned int count = 0;
    glthread_t *glthreadptr = nullptr;

    ITERATE_GLTHREAD_BEGIN(base_glthread, glthreadptr){
        count++;
    } ITERATE_GLTHREAD_END(base_glthread, glthreadptr);
    return count;
}

/*Insert a node to a priority linked list based on the block size it holds*/
void
glthread_priority_insert(glthread_t *base_glthread, 
                         glthread_t *glthread,
                         int (*comp_fn)(void *, void *),
                         int offset) {


    glthread_t *curr = nullptr,
               *prev = nullptr;

    init_glthread(glthread);

    /*If the whole list is empty, add the node at the rhs of the head*/
    if(IS_GLTHREAD_LIST_EMPTY(base_glthread)){
        glthread_add_next(base_glthread, glthread);
        return;
    }

    /*Only one node, and compare the block size of two blocks*/
    if(base_glthread->right && !base_glthread->right->right){
        if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(base_glthread->right, offset), 
                GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthread, offset)) == -1){
            glthread_add_next(base_glthread->right, glthread); // rhs node is larger
        }
        else{
            glthread_add_next(base_glthread, glthread); // new node is larger
        }
        return;
    }

    /*Add at the head of the list*/
    if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthread, offset), 
            GLTHREAD_GET_USER_DATA_FROM_OFFSET(base_glthread->right, offset)) == -1){
        glthread_add_next(base_glthread, glthread);
        return;
    }

    /*Add in the middle of the list*/
    ITERATE_GLTHREAD_BEGIN(base_glthread, curr){

        if(comp_fn(GLTHREAD_GET_USER_DATA_FROM_OFFSET(glthread, offset), 
                GLTHREAD_GET_USER_DATA_FROM_OFFSET(curr, offset)) != -1){
            prev = curr;
            continue;
        }

        glthread_add_next(curr, glthread);
        return;

    }ITERATE_GLTHREAD_END(base_glthread, curr);

    /*Add in the end*/
    glthread_add_next(prev, glthread);
} 

#if 0
void *
gl_thread_search(glthread_t *base_glthread, 
                 void *(*thread_to_struct_fn)(glthread_t *), 
                 void *key, 
                 int (*comparison_fn)(void *, void *)){

    return NULL;
}
#endif