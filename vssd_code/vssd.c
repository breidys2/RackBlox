#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "vssd.h"

static const int BLK_SZ = PAGE_SIZE*4*256;

//Create a regular vssd
//Probably want to keep these constructors separate
vssd_t * alloc_regular_vssd(int chl) {
    
    //Vars we use later in this function
    int i, j, alloc;

    //Allocate the vssd
    vssd_t * ret_vssd = malloc(sizeof(vssd_t));

    //Set the vssd_type
    ret_vssd->vssd_type = regular;

    //Also set the current max address to avoid out of bounds
    ret_vssd->max_addr = CHL_BLK_NUM * chl;

    //Regular vssd wants to create a mapping table
    ret_vssd->mapping_table_v = calloc(ret_vssd->max_addr,sizeof(uint32_t));
    ret_vssd->valid_pages_v = calloc(ret_vssd->max_addr,sizeof(uint32_t));
    ret_vssd->bit_map_v = calloc(ret_vssd->max_addr,sizeof(uint64_t*));
    //For debug, which channels did we successfully allocate 
    //Also useful in case we fail to allocate channels
    ret_vssd->allocated_chl = malloc(chl * sizeof(int));
    for(j =0; j < chl; j++) ret_vssd->allocated_chl[j] = -1;

    //Alloc the lists for block management, alloc for allocated blocks, free for those not allocated
    ret_vssd->alloc_block_list = calloc(16,sizeof(node_t *));
    ret_vssd->free_block_list = calloc(16,sizeof(node_t *));

    //Not sure this is needed
    for(i = 0; i < 16; i++) {
        pthread_mutex_init(&(ret_vssd->free_locks[i]),NULL);
        pthread_mutex_init(&(ret_vssd->alloc_locks[i]),NULL);
    }

    //Alloc lba queue
    ret_vssd->lba_queue = malloc(sizeof(Queue *));
    ret_vssd->lba_queue = createQueue(DEVICE_BLK_NUM);

    //Set the space and bandwidth of this vssd
    ret_vssd->bandwidth = chl;
    ret_vssd->space = (uint64_t)chl * CHL_BLK_NUM;

    //Only need one ghost slot
    ret_vssd->ghost = calloc(1,sizeof(vssd_t*));

    //Set up the lba queue with corresponding addresses
    for(j = 0; j < CHL_BLK_NUM * chl; j++) enqueue(ret_vssd->lba_queue, j);

    //Go through and allocate channels
    alloc = 0;
    pthread_t threads[16];
    bundle_t * th_info = malloc(16 * sizeof(bundle_t));
    for(i = 0; i < 16; i++) {
        threads[i] = 0;
    }
    int rc;
    for(i = 0; i < 16 && alloc < chl; i++) {
        //Continue if the channel is already allocated
        if ((alloc_chl & (1 << i)) > 0) continue;
        //Set the allocated bit to track allocated channels
        alloc_chl |= (1 << i);
        //Record this as an allocated channel for this vssd 
        ret_vssd->allocated_chl[alloc++] = i;
        th_info[i].a = ret_vssd;
        th_info[i].index = i;
        if ((rc = pthread_create(&threads[i], NULL, prep_chl_v, &th_info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return EXIT_FAILURE;
        } 
        //else fprintf(stdout, "Spawned Thread: %d\n", i);
    }

    for(i = 0; i < 16; i++) {
        if (threads[i] == 0) continue;
        pthread_join(threads[i], NULL);
    }
    free(th_info);
    //In the unlikely event of failure, we dealloc and return failure
    if (alloc < chl) {
        free_regular_vssd(ret_vssd);
        //Return failure
        return NULL;
    }
    //Return vssd on success
    return ret_vssd;
}

//Takes the pointer to a vssd and frees it
void free_regular_vssd(vssd_t * v) {

    int i;
    //Iterate over all channels and free those allocated to us
    for(i = 0; i < v->bandwidth; i++) {

        int f_chl = v->allocated_chl[i];
        if (f_chl == -1) break;

        //Remove from allocated channels
        alloc_chl ^= (1 << f_chl);

        //Free the lists
        free_blk_list_v(v->alloc_block_list[f_chl]);
        free_blk_list_v(v->free_block_list[f_chl]);
    }

    //Free the remaining malloc'ed items in the vssd
    free(v->mapping_table_v);
    free(v->valid_pages_v);
    for (i = 0; i < v->max_addr; i++) {
        if (v->bit_map_v[i] != NULL) {
            free(v->bit_map_v[i]);
        }
    }
    free(v->bit_map_v);
    free(v->allocated_chl);
    free(v->alloc_block_list);
    free(v->free_block_list);
    free(v->ghost);
    while(dequeue(v->lba_queue) >= 0);
    free(v->lba_queue);
    free(v);
}

//Grabs a pba, lba, maps them and returns the lba
//This function is generic across vssd types
uint32_t alloc_block_v(vssd_t * v, int chl_id) {

    uint32_t ppa_addr;
    int lba_addr;

    //lock the list
    pthread_mutex_lock(&v->free_locks[chl_id]);
    //Grab the block
    node_t * free_blk = pop_list_v(&v->free_block_list[chl_id]);
    v->free_block_sz--;
    pthread_mutex_unlock(&v->free_locks[chl_id]);
    if (free_blk) ppa_addr = free_blk->ppa;
    else {
        printf("Cannot allocate free block\n");
        exit(0);
    }

    //Grab a logical block address
    lba_addr = dequeue(v->lba_queue);
    if (lba_addr == -1) {
        printf("ERROR: cannot get LBA\n");
        exit(0);
    }

    //Map them together
    v->mapping_table_v[lba_addr] = ppa_addr;
    v->valid_pages_v[lba_addr] = 1024;
    v->bit_map_v[lba_addr] = malloc(1024);

    //add to alloc block list, don't need to make a new node since we keep the old one
    add_list_v(&v->alloc_block_list[chl_id], free_blk);

    //Return the lba
    return lba_addr;
}

void * prep_chl_v(void * args) {
    bundle_t * info = (bundle_t *)args;
    vssd_t * ret_vssd = info->a;
    int i,j,k,l;
    i = info->index;
    //Setup up some tracking information
    for(j=0; j < CFG_NAND_LUN_NUM; j++) {
        for(k=0; k < CFG_NAND_BLOCK_NUM; k++) {
            //if(k >= CFG_NAND_BLOCK_NUM - RESERVED_SUPERBLK) {
            //    superblk_bad[chl_id][j][k-(CFG_NAND_BLOCK_NUM - RESERVED_SUPERBLK)] = 0;
            //    superblk_ers_cnt[chl_id][j][k-(CFG_NAND_BLOCK_NUM - RESERVED_SUPERBLK)] = 0;
            //}
            for(l=0; l < CFG_NAND_PLANE_NUM; l++) {
                blk_status[i][j][k][l] = 0;
                blk_ers_cnt[i][j][k][l] = 0;
                blk_bad[i][j][k][l] = 0;
            }
        }
    }

    //allocate the channel, reads badblock file, preps blocks, then adds to the free list 
    erase_all_blk_chl_v(i,ret_vssd);
}

void erase_blks_v(vssd_t * v, int lim) {
    bundle_t * th_info = malloc(get_bw(v) * sizeof(bundle_t));
    pthread_t threads[16];
    int i,rc;
    for(i = 0; i < get_bw(v); i++) {
        th_info[i].a = v;
        th_info[i].index = v->allocated_chl[i];
        th_info[i].blks = lim;
        if ((rc = pthread_create(&threads[i], NULL, ret_blks_v, &th_info[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return EXIT_FAILURE;
        } 
        //else fprintf(stdout, "Spawned Thread: %d\n", i);
    }
    for(i = 0; i < get_bw(v); i++) {
        pthread_join(threads[i], NULL);
    }
    free(th_info);
}

double get_gc_thresh(vssd_t * v) {
    return v->free_block_sz/v->tot_blocks;
}

void invalidate_page(vssd_t *v, uint32_t lba) {
    uint32_t page = lba%1024;
    lba/=1024;
    v->valid_pages_v[lba]--;
    int bit_ind = page /64;
    int bit_off = page % 64;
    v->bit_map_v[lba][bit_ind] ^= (1L << bit_off);
}

void do_gc(vssd_t * v, uint32_t* mapping, uint32_t* rev_mapping) {
    int i,j;
    //Enumerate the allocated blocks
    int chl = v->allocated_chl[0];
    int cnt = 0;
    char gc_buf[PAGE_SIZE];
    char gc_metabuf[META_SIZE];
    node_t* cur_node = v->alloc_block_list[chl];
    int cur_pg = 0;
    int cur_lba = alloc_block_v(v,chl);
    while(cnt < gc_lim) {
        if (cur_node == NULL) break;
        uint32_t ppa = cur_node->ppa;
        if (v->valid_pages_v[ppa] < gc_thresh) {
            //GC this block
            for(i = 0; i < 16; i++) {
                for(j = 0; j < 64; j++) {
                    if (((1L << 64) & v->bit_map_v[ppa][i]) > 0) {
                        //Valid page, read and copy
                        int p_ind = i * 64 + j;
                        read_page_v(v, p_ind, ppa, i, gc_buf, gc_metabuf);
                        write_page_v(v, cur_lba, cur_pg, gc_buf, gc_metabuf);
                        //Redo mappings
                        mapping[rev_mapping[ppa + p_ind]] = cur_lba + cur_pg;
                        rev_mapping[cur_lba + cur_pg] = rev_mapping[ppa+p_ind];
                        rev_mapping[ppa + p_ind] = 0;
                        cur_pg++;
                        if (cur_pg == 1024) {
                            cur_pg = 0;
                            cur_lba = alloc_block_v(v,chl);
                        }
                    }
                }
            }
            erase_blk_v(ppa);
            free(v->bit_map_v[ppa]);
            v->bit_map_v[ppa] = NULL;
            v->valid_pages_v[ppa] = 0;
        }
        cur_node = cur_node->next;
    }
}

void * ret_blks_v(void * args) {
    bundle_t * info = (bundle_t *) args;
    int cur_chl = info->index;
    vssd_t * v = info->a;
    int lim = info->blks;
    int cnt = 0;
    //Erase and return allocated blocks
    while(cnt < lim) {
        //Grab block from ghost vssd
        node_t * f_block = pop_list_v(&v->alloc_block_list[cur_chl]);
        if (!f_block) break;

        //Erase the block
        erase_blk_v(f_block->ppa);
        free(v->bit_map_v[f_block->ppa]);
        v->bit_map_v[f_block->ppa] = NULL;
        v->valid_pages_v[f_block->ppa] = 0;
        cnt++;

        //Return to free list
        add_list_v(&v->free_block_list[cur_chl], f_block);
        v->free_block_sz++;
    }
}


//==================================================
//         HELPER FUNCTIONS
//==================================================
//Copied helper function
//Free the entire list
int free_blk_list_v(node_t* head) {
    node_t* current = NULL;
    node_t * temp = NULL;
    current = head;
    while(current != NULL) {
        temp = current;
        current = current->next;
        temp->next = NULL;
        free(temp);
    }
    return 0;
}
//Get the length of the list
int list_len_v(node_t * head) {
    node_t * current = head;
    int cnt = 0;
    while(current != NULL) {
        cnt++;
        current = current->next;
    }
    return cnt;
}
//Return the head of the list and remove it 
node_t * pop_list_v(node_t ** head) {
    node_t * current = NULL;
    if (*head == NULL) return NULL;
    else {
        current = *head;
        *head = current->next;
        current->next = NULL;
        return current;
    }
}
//Head is the head of the list
//node is the node_t being added
int add_list_v(node_t ** head, node_t* node) {
    node->next = NULL;
    node_t* current = NULL;
    if(*head == NULL) {
        *head = node;	
    }
    else{
        if((*head)->data <= node->data) {
            node->next = *head;
            *head = node;
        }
        else{
            current = *head;
            while(current->next != NULL && current->next->data > node->data){
                current = current->next;
            }
            node->next = current->next;
            current->next = node;
        }
    }
    return 0;
}
//Get the bandwidth of the vssd
uint32_t get_bw (vssd_t * v) {
    return v->bandwidth;
}
