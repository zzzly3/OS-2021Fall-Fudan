#pragma once

#ifndef _CORE_MEMORY_MANAGE_
#define _CORE_MEMORY_MANAGE_

typedef struct {
    void *struct_ptr;
    void (*page_init)(void *datastructure_ptr, void *start, void *end);
    void *(*page_alloc)(void *datastructure_ptr);
    void (*page_free)(void *datastructure_ptr, void *page_address);
} PMemory;

typedef struct {
    void *next;
} FreeListNode;

void init_memory_manager(void);
void free_range(void *start, void *end);
void *kalloc(void);
void kfree(void *page_address);

#define PPN_MAX (0x3F000000/4096)

#endif