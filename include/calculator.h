#ifndef __CALCULATOR_H__
#define __CALCULATOR_H__

typedef enum status
{
    PROPER_INPUT,
    ILLEGAL_INPUT,
    ILLEGAL_ALGEBRIC_ACTION,
    MEMORY_ERROR,
    STATUS_COUNT
}status_t;

double Calculator(const char *str, status_t *status);
/*
Returnes the final result of the expression, "status" (out param)
is updated appropriately.
When "status" is not "PROPER_INPUT", 0 will be returned.
white spaces are allowed.
*/

#endif /* __CALCULATOR_H__ */

