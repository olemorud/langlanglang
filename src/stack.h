
#pragma once

#include <stdlib.h>
#include <stdio.h>

#define STACK_MAX 256

typedef struct fixed_stack {
	void* vals[STACK_MAX];
	size_t top;
} FixedStack;

#define STACK_INIT { 0 }

#define stack_push(s, v) stack_push_((s), (void*)(v))
static inline void stack_push_(FixedStack* s, void* val)
{
	if (s->top >= STACK_MAX) {
		fprintf(stderr, "static stack capacity exceeded");
		exit(EXIT_FAILURE);
	}
	s->vals[s->top++] = val;
}

static inline void* stack_top(FixedStack* s)
{
	return s->vals[s->top - 1];
}

static inline void* stack_pop(FixedStack* s)
{
	return s->vals[--s->top];
}

static inline bool stack_empty(FixedStack* s)
{
	return s->top == 0;
}

static inline size_t stack_len(FixedStack* s)
{
	return s->top;
}
