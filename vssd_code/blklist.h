#ifndef _BLKLIST_H
#define _BLKLIST_H

#include "nexus.h"

typedef struct node{
    u32 ppa;
    int data;
    struct node* next;
} node_t;

//node_t *alloc_blk_list[CFG_NAND_CHANNEL_NUM] = {NULL};
//node_t *free_blk_list[CFG_NAND_CHANNEL_NUM] = {NULL};
//node_t *dead_blk_list[CFG_NAND_CHANNEL_NUM] = {NULL};

#endif
