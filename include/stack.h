#ifndef __STACK_H__
#define __STACK_H__

#include <stddef.h> /* size_t */

typedef struct stack stack_t; 


/*******************************************************************/

stack_t *StackCreate(size_t element_size, size_t num_of_elements);

void StackDestroy(stack_t *stack);

const void *StackPeek(const stack_t *stack);
/*
If called with an empty stack - undefined behaviour.
*/

void StackPush(stack_t *stack, const void *data);
/*
If called with a full stack - undefined behaviour.
*/

void StackPop(stack_t *stack);
/*
If called with an empty stack - undefined behaviour.
*/

size_t StackSize(stack_t *stack);
/*
Time complexity is O(1).
*/


#endif /* __STACK_H__ */

