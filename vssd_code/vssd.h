#ifndef _VSSD_H
#define _VSSD_H

#include "nexus.h"
#include "blklist.h"
#include "queue.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#define SUCCESS 0 
#define FAIL 0 
#define CHL_BLK_NUM (CFG_NAND_BLOCK_NUM * CFG_NAND_PLANE_NUM * CFG_NAND_LUN_NUM)

static uint16_t alloc_chl;

typedef enum {regular, harvest, ghost, pghost} type;

typedef struct vssd {
    //Metadata
    uint32_t bandwidth;      //Bandwidth of the vssd
    uint64_t space;          //Total space of the vssd
    uint32_t duration;       //Total tagged duration of the vssd
    struct vssd* home;  //The home pointer for this vssd
    struct vssd** ghost;//List of ghost pointers for this vssd
    type vssd_type;//What type of vssd is this, mostly for QoL when coding
    //Necessary data structures
    uint32_t * mapping_table_v; //Internal mapping table 
    uint32_t * valid_pages_v;
    uint64_t ** bit_map_v;

    //Assorted implementation related stuff
    int ghosts;                 //Number of ghosts (if pghost)
    int* allocated_chl;         //List of allocated channels of this ssd
    node_t ** alloc_block_list; //List of allocated physical blocks
    int free_block_sz;
    int tot_blocks;
    node_t ** free_block_list;  //List of free physical blocks
    pthread_mutex_t free_locks[16];
    pthread_mutex_t alloc_locks[16];
    Queue * lba_queue;          //Set of LBAs that are still available.
    uint32_t max_addr;
} vssd_t;

static int gc_lim = 10;
static double gc_thresh = 256; 

//This order should matter here, since channel needs the vssd defined
#include "channel.h"

//Vssd alloc/free functions
vssd_t * alloc_regular_vssd(int chl);
void free_regular_vssd(vssd_t * v);

//child/pghost funcs
void add_ghost_vssd(vssd_t * pghost, vssd_t * ghost);
void remove_ghost_vssd(vssd_t * pghost, vssd_t * ghost);

//blk list functions
int free_blk_list_v(node_t* head);
int add_list_v(node_t ** head, node_t* node);
node_t * pop_list_v(node_t ** head);
int list_len_v(node_t * head);

//vssd utilization functions
uint32_t alloc_block_v(vssd_t * v, int chl_id);
void * ret_chl_v(void * args);
void * prep_chl_v(void * args);
void * harvest_chl_v(void * args);
void * ret_blks_v(void * args);
void erase_blks_v(vssd_t * v, int lim);

typedef struct _bundle_t {
    int index;
    int blks;
    vssd_t * a;
    vssd_t * b;
} bundle_t;

//vssd metadata function
void set_bw(vssd_t * v, uint32_t bw);
uint32_t get_bw (vssd_t * v);
double get_gc_thresh(vssd_t * v);
void invalidate_page(vssd_t *v, uint32_t lba);
void do_gc(vssd_t * v, uint32_t* mapping, uint32_t* rev_mapping);
#endif
