#include <stdlib.h> /* malloc */
#include <string.h> /* memcpy */

#include "stack.h"

struct stack
{
    size_t size_of_element;
    char *bottom_ptr;
    char *top_ptr;
};


/*******************************************************************/

stack_t *StackCreate(size_t size_of_element, size_t num_of_elements)
{
    stack_t *new_stack = NULL;
    char *stack_memory = NULL;
    
    stack_memory = malloc(num_of_elements * size_of_element);
    if (NULL == stack_memory)
    {
        return (NULL);
    }

    new_stack = malloc(sizeof(stack_t));
    if (NULL == new_stack)
    {
        free(stack_memory);
        stack_memory = NULL;

        return (NULL);
    }
    
    new_stack->bottom_ptr = stack_memory;
    new_stack->top_ptr = stack_memory;
    new_stack->size_of_element = size_of_element;
    
    return (new_stack);
}

/***********/
void StackDestroy(stack_t *stack)
{
    free(stack->bottom_ptr);
    stack->bottom_ptr = NULL;                                    
    stack->top_ptr = NULL; 
                          
    free(stack);
    stack = NULL;                        
}

/***********/
const void *StackPeek(const stack_t *stack)
{
    char *last_element_ptr = stack->top_ptr - stack->size_of_element;
    
    /* implicitly converted to void* */
    return (last_element_ptr);
}

/***********/
void StackPush(stack_t *stack, const void *data)
{
    memcpy(stack->top_ptr, data, stack->size_of_element); 
    stack->top_ptr += stack->size_of_element;
}

/***********/
void StackPop(stack_t *stack)
{
    stack->top_ptr -= stack->size_of_element;
}

/***********/
size_t StackSize(stack_t *stack)
{
    size_t stack_size = (size_t)(stack->top_ptr - stack->bottom_ptr) /
                         stack->size_of_element;
    
    return (stack_size);
}

