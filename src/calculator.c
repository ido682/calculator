#include <stdlib.h> /* strtod */
#include <sys/types.h> /* ssizt_t */
#include <assert.h> /* assert */
#include <string.h> /* strchr */
#include <math.h> /* pow */

#include "calculator.h"
#include "stack.h"

#define NUM_OF_CHARS 256
#define MAX_SIZE 1024

typedef struct ops_characteristics ops_characteristics_t;
typedef struct ar_data ar_data_t;
typedef struct transition transition_t;

/* "op" is for "operator", "ar" is for "action routine" */
typedef double (*op_func_t)(double, double, status_t *);
typedef status_t (*ar_func_t)(ar_data_t *);

typedef enum STATES_ENUMS
{
    GET_NUM = 0,
    GET_OP = 1,
    FIN = 2,
    FAIL = 3
} state_t;

/* order mustn't be changed!! */
typedef enum OPERATORS_ENUMS
{
    STUB,
    OPEN_PAR,    /* ( */
    NULL_TER,    /* = */
    ADD,         /* + */
    SUBTRACT,    /* - */
    MULTIPLY,    /* * */
    DIVIDE,      /* / */
    EMPOWER,     /* ^ */
    OPERATORS_ENUMS_COUNT      
} operator_t;

typedef enum ASSOCIATIVITY_ENUMS
{
    RIGHT_TO_LEFT = 0,
    LEFT_TO_RIGHT = 1,
    ASSOCIATIVITY_ENUMS_COUNT   
} associativity_t;

/* PEMDAS - order of operations.
stubs are for ShouldCalculate() */
typedef enum PEMDAS_ENUMS
{
    PEMDAS_STUB,
    EMPTY0,
    PEMDAS_OPEN_PAR,
    EMPTY1,
    PEMDAS_ADD_SUBTRACT,
    EMPTY2,
    PEMDAS_MULTIPLY_DIVIDE,
    EMPTY3,
    PEMDAS_EMPOWER,
    PEMDAS_ENUMS_COUNT   
} pemdas_t;


struct ops_characteristics
{
    pemdas_t pemdas;
    associativity_t associativity;
};

struct transition
{
    char *chars_list;
    ar_func_t ar_func;
    state_t next_state;
};

struct ar_data
{
    const char *str_ptr;
    stack_t *stack_numbers;
    stack_t *stack_operators;
    ssize_t pars_ctr;
    operator_t *lut_operators;
    op_func_t *lut_operations;
    status_t status;
};

/************  STATIC FUNCTIONS  ****************/
static status_t GoToFail(ar_data_t *ar_data);
static status_t SkipChar(ar_data_t *ar_data);
static status_t PushNumberToStack(ar_data_t *ar_data);
static status_t DoOperation(ar_data_t *ar_data);
static status_t CalFinalRes(ar_data_t *ar_data);
static status_t PushOpenPar(ar_data_t *ar_data);
static status_t ClosePars(ar_data_t *ar_data);

static double Add(double first, double second, status_t *status);
static double Subtract(double first, double second, status_t *status);
static double Multiply(double first, double second, status_t *status);
static double Divide(double first, double second, status_t *status);
static double Empower(double first, double second, status_t *status);

static double PeekAndPopNumber(stack_t *stack_numbers);
static double PeekNumber(stack_t *stack_numbers);
static operator_t PeekAndPopOperator(stack_t *stack_operators);
static operator_t PeekOperator(stack_t *stack_operators);

static int ShouldCalculate(operator_t new_op, operator_t prev_op);
static void CalculateOne(ar_data_t *ar_data);
static void InitTransitions(transition_t lut_transitions[2][NUM_OF_CHARS],
                            state_t state, transition_t transition);
static void InitAll(void);


/* static LUTs */
static operator_t lut_operators[NUM_OF_CHARS] = {STUB};
static op_func_t lut_operations[OPERATORS_ENUMS_COUNT] = {NULL};
static transition_t lut_transitions[2][NUM_OF_CHARS] = {NULL};
static ops_characteristics_t lut_ops_characteristics[OPERATORS_ENUMS_COUNT] =
                             {0};
static int is_first_run = 1;

/* Lists of relevant chars - will be used to initialize
('@' is located in the end to terminaite loops in initialization) */
static char all_white_spaces[] = {' ', '\t', '\n', '\v', '\f', '\r', '@'};
static char all_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            '+', '-', '@'};
static char all_open_par[] = {'(', '@'};
static char all_close_par[] = {')', '@'};
static char all_operators[] = {'+', '-', '*', '/', '^', '@'};
static char all_null_ter[] = {'\0', '@'};



/*********************************
*       Definition Table         *
* *******************************/
/*
Name (current state and event)
____Ptr to chars list___________Action rotine_______________Next state__
*/
static transition_t get_num_white_spaces =
    {all_white_spaces,          SkipChar,                   GET_NUM  };

static transition_t get_num_numbers =
    {all_digits,                PushNumberToStack,          GET_OP   };

static transition_t get_num_open_par =
    {all_open_par,              PushOpenPar,                GET_NUM  };

static transition_t get_num_default =
    {NULL,                      GoToFail,                   FAIL     };

/*************/

static transition_t get_op_white_spaces =
    {all_white_spaces,          SkipChar,                   GET_OP   };

static transition_t get_op_operators =
    {all_operators,             DoOperation,                GET_NUM  };

static transition_t get_op_close_par =
    {all_close_par,             ClosePars,                  GET_OP   };

static transition_t get_op_null_ter =
    {all_null_ter,              CalFinalRes,                FIN      };

static transition_t get_op_default =
    {NULL,                      GoToFail,                   FAIL     };

/*************/

/* setting characteristics */
static ops_characteristics_t characteristics_open_par =
    {PEMDAS_OPEN_PAR,               LEFT_TO_RIGHT};
static ops_characteristics_t characteristics_add =
    {PEMDAS_ADD_SUBTRACT,           LEFT_TO_RIGHT};
static ops_characteristics_t characteristics_subtract =
    {PEMDAS_ADD_SUBTRACT,           LEFT_TO_RIGHT};
static ops_characteristics_t characteristics_multiply =
    {PEMDAS_MULTIPLY_DIVIDE,        LEFT_TO_RIGHT};
static ops_characteristics_t characteristics_divide =
    {PEMDAS_MULTIPLY_DIVIDE,        LEFT_TO_RIGHT};
static ops_characteristics_t characteristics_empower =
    {PEMDAS_EMPOWER,                RIGHT_TO_LEFT};




/*******************************************************************/

double Calculator(const char *expression, status_t *status)
{
    double result = 0;
    status_t internal_status = PROPER_INPUT;
    state_t cur_state = GET_NUM;
    const char *str_ptr = expression;
    ar_data_t ar_data = {0};/* all data needed by Action Routine functions */

    stack_t *stack_numbers = NULL;
    stack_t *stack_operators = NULL;
    operator_t stub = STUB;
    
    assert(expression != NULL);

    stack_numbers = StackCreate(sizeof(double), MAX_SIZE);
    if (NULL == stack_numbers)
    {
        *status = MEMORY_ERROR;

        return (0);
    }

    stack_operators = StackCreate(sizeof(operator_t), MAX_SIZE);
    if (NULL == stack_operators)
    {
        StackDestroy(stack_numbers);
        stack_numbers = NULL;

        *status = MEMORY_ERROR;

        return (0);
    }
    
    if (is_first_run)
    {
        InitAll();        
    }

    ar_data.str_ptr = str_ptr;
    ar_data.stack_numbers = stack_numbers;
    ar_data.stack_operators = stack_operators;
    ar_data.pars_ctr = 0;
    ar_data.lut_operators = lut_operators;
    ar_data.lut_operations = lut_operations;
    ar_data.status = PROPER_INPUT;

    StackPush(stack_operators, &stub);

    /* any state but FAIL or FIN */
    while (cur_state <= 1) 
    {
        char curr_char = *ar_data.str_ptr;
        transition_t transition =
            lut_transitions[cur_state][(int)curr_char];

        internal_status = transition.ar_func(&ar_data);
        cur_state = (PROPER_INPUT == internal_status) ?
                    (transition.next_state) : (FAIL);
    }
    
    /* return 0 on failure */
    result = (FIN == cur_state) ? (PeekNumber(stack_numbers)) : (0);

    StackDestroy(stack_numbers);
    stack_numbers = NULL;
    StackDestroy(stack_operators);
    stack_operators = NULL;

    *status = internal_status;

    return (result);
}




/*********************************
*    Action Routine Functions    *
*********************************/

static status_t GoToFail(ar_data_t *ar_data)
{
    ar_data->status = ILLEGAL_INPUT;
    
    return (ar_data->status);
}

/*************/
static status_t CalFinalRes(ar_data_t *ar_data)
{
    operator_t prev_op = PeekOperator(ar_data->stack_operators);

    /* 0 (PROPER_INPUT) if 0 parantheses left, 1 (ILLEGAL_INPUT) otherwise */
    ar_data->status = (ar_data->pars_ctr != 0);

    /* keep calculating until the end or until bad status */
    while ((prev_op != STUB) && (PROPER_INPUT == ar_data->status))
    {
        CalculateOne(ar_data);
        prev_op = PeekOperator(ar_data->stack_operators);
    }

    return (ar_data->status);
}

/*************/
static status_t SkipChar(ar_data_t *ar_data)
{
    ++ar_data->str_ptr;

    return (PROPER_INPUT);
}

/*************/
static status_t PushNumberToStack(ar_data_t *ar_data)
{
    double number = 0;
    const char *str_ptr_before_change = ar_data->str_ptr;

    ar_data->status = PROPER_INPUT;

    number = strtod(ar_data->str_ptr, (char **)&ar_data->str_ptr);

    /* status is set to 1 (ILLEGAL_INPUT) if no number has been detected
    by strtod, 0 (PROPER_INPUT) otherwise */
    ar_data->status =
        ((ar_data->str_ptr == str_ptr_before_change) && (0 == number));

    StackPush(ar_data->stack_numbers, &number);

    return (ar_data->status);
}

/*************/
static status_t DoOperation(ar_data_t *ar_data)
{
    char curr_char = *ar_data->str_ptr;
    operator_t new_op = ar_data->lut_operators[(int)curr_char];
    operator_t prev_op = PeekOperator(ar_data->stack_operators);
    
    /* keep calculating until PEMDAS doesn't require, or until bad status */
    while ((ShouldCalculate(new_op, prev_op)) &&
          (PROPER_INPUT == ar_data->status))
    {
        CalculateOne(ar_data);
        prev_op = PeekOperator(ar_data->stack_operators);
    }
    
    StackPush(ar_data->stack_operators, &new_op);

    ++ar_data->str_ptr;

    return (ar_data->status);
}

/*************/
static status_t PushOpenPar(ar_data_t *ar_data)
{
    char curr_char = *ar_data->str_ptr;
    operator_t open_par = ar_data->lut_operators[(int)curr_char];
    
    StackPush(ar_data->stack_operators, &open_par);
    ++ar_data->pars_ctr;
    ++ar_data->str_ptr;

    return (PROPER_INPUT);
}

/*************/
static status_t ClosePars(ar_data_t *ar_data)
{
    operator_t prev_op = PeekOperator(ar_data->stack_operators);

    /* 0 (PROPER_INPUT) if there are any open parantheses,
    1 (ILLEGAL_INPUT) otherwise */
    ar_data->status = (ar_data->pars_ctr == 0);
    
    /* keep calculating until encounters open par, or until bad status */
    while ((prev_op != OPEN_PAR) && (PROPER_INPUT == ar_data->status))
    {
        CalculateOne(ar_data);
        prev_op = PeekOperator(ar_data->stack_operators);
    }

    StackPop(ar_data->stack_operators);
    --ar_data->pars_ctr;
    ++ar_data->str_ptr;
    
    return (ar_data->status);
}


/*********************************
*     Arithmetical Functions     *
*********************************/

static double Add(double first, double second, status_t *status)
{
    /* the function is necessarily called when "status" is PROPER_INPUT
    and no change is needed */
    (void)status;
    
    return (first + second);
}

/*************/
static double Subtract(double first, double second, status_t *status)
{
    /* the function is necessarily called when "status" is PROPER_INPUT
    and no change is needed */
    (void)status;
    
    return (first - second);
}

/*************/
static double Multiply(double first, double second, status_t *status)
{
    /* the function is necessarily called when "status" is PROPER_INPUT
    and no change is needed */
    (void)status;
    
    return (first * second);
}

/*************/
static double Divide(double first, double second, status_t *status)
{
    /* avoid crash when intend to divide by zero */
    if (0 == second)
    {
        *status = ILLEGAL_ALGEBRIC_ACTION;

        return (0);
    }
    
    return (first / second);
}

/*************/
static double Empower(double first, double second, status_t *status)
{
	/* avoid crash when intend to empower 0 by a negative number */
    if ((0 == first) && (second < 0))
    {
        *status = ILLEGAL_ALGEBRIC_ACTION;

        return (0);
    }

    return (pow(first, second));
}


/*********************************
*          Stack Functions       *
*********************************/

static double PeekAndPopNumber(stack_t *stack_numbers)
{
    double number = *(double *)StackPeek(stack_numbers);
    StackPop(stack_numbers);

    return (number);
}

/*************/
static double PeekNumber(stack_t *stack_numbers)
{
    double number = *(double *)StackPeek(stack_numbers);

    return (number);
}

/*************/
static operator_t PeekAndPopOperator(stack_t *stack_operators)
{
    operator_t operator = *(operator_t *)StackPeek(stack_operators);
    StackPop(stack_operators);

    return (operator);
}

/*************/
static operator_t PeekOperator(stack_t *stack_operators)
{
    operator_t operator = *(operator_t *)StackPeek(stack_operators);

    return (operator);
}


/*********************************
*          More Functions        *
*********************************/

static void CalculateOne(ar_data_t *ar_data)
{
    double second = PeekAndPopNumber(ar_data->stack_numbers);
    double first = PeekAndPopNumber(ar_data->stack_numbers);
    operator_t operator = PeekAndPopOperator(ar_data->stack_operators);

    double result =
        ar_data->lut_operations[operator](first, second, &ar_data->status);
    StackPush(ar_data->stack_numbers, &result);
}

/*************/
static int ShouldCalculate(operator_t new_op, operator_t prev_op)
{
    int rate_of_prev = lut_ops_characteristics[prev_op].pemdas +
                       lut_ops_characteristics[prev_op].associativity;
    int rate_of_new =  lut_ops_characteristics[new_op].pemdas;

    return (rate_of_prev > rate_of_new);
}

/*************/
static void InitTransitions(transition_t lut_transitions[2][NUM_OF_CHARS],
                            state_t state, transition_t transition)
{
    char *char_ptr = transition.chars_list;

    while (*char_ptr != '@')
    {
        lut_transitions[state][(int)*char_ptr] = transition;
        ++char_ptr;
    }
}

/*************/
static void InitAll(void)
{
    int i = 0;

    /* first initialize of operators LUT */
    lut_operators['+'] = ADD;
    lut_operators['-'] = SUBTRACT;
    lut_operators['*'] = MULTIPLY;
    lut_operators['/'] = DIVIDE;
    lut_operators['^'] = EMPOWER;
    lut_operators['('] = OPEN_PAR;
    lut_operators['\0'] = NULL_TER;

    /* first initialize of operations LUT */
    lut_operations[ADD] = Add;
    lut_operations[SUBTRACT] = Subtract;
    lut_operations[MULTIPLY] = Multiply;
    lut_operations[DIVIDE] = Divide;
    lut_operations[EMPOWER] = Empower;
    lut_operations[NULL_TER] = NULL; 

    lut_ops_characteristics[OPEN_PAR] = characteristics_open_par;
    lut_ops_characteristics[ADD] = characteristics_add;
    lut_ops_characteristics[SUBTRACT] = characteristics_subtract;
    lut_ops_characteristics[MULTIPLY] = characteristics_multiply;
    lut_ops_characteristics[DIVIDE] = characteristics_divide;
    lut_ops_characteristics[EMPOWER] = characteristics_empower;

    /* first initialize of GET_NUM and GET_OP LUT */
    for (i = 0; i < NUM_OF_CHARS; ++i)
    {
        lut_transitions[GET_NUM][i] = get_num_default;
        lut_transitions[GET_OP][i] = get_op_default;
    }

    InitTransitions(lut_transitions, GET_NUM, get_num_white_spaces);
    InitTransitions(lut_transitions, GET_NUM, get_num_numbers);
    InitTransitions(lut_transitions, GET_NUM, get_num_open_par);

    InitTransitions(lut_transitions, GET_OP, get_op_white_spaces);
    InitTransitions(lut_transitions, GET_OP, get_op_operators);
    InitTransitions(lut_transitions, GET_OP, get_op_close_par);
    InitTransitions(lut_transitions, GET_OP, get_op_null_ter);

    is_first_run = 0;    
}

