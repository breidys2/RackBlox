#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "nexus.h"
#include "queue.h"

#define TOTAL_NUM_CHANNEL 16
#define SECTOR_SIZE 4096
#define FUN_SUCCESS 0
#define FUN_FAILURE (-1)
#define SWITCH_THRESHOLD  20
#define RESERVED_SUPERBLK 100

#define DEVICE_BLK_NUM (CFG_NAND_BLOCK_NUM * CFG_NAND_PLANE_NUM * CFG_NAND_LUN_NUM * CFG_NAND_CHANNEL_NUM)
#define CHL_BLK_NUM (CFG_NAND_BLOCK_NUM * CFG_NAND_PLANE_NUM * CFG_NAND_LUN_NUM)



void* mapping_table = NULL;
u16 chl_allocated = 0;
u8  blk_status[CFG_NAND_CHANNEL_NUM][CFG_NAND_LUN_NUM][CFG_NAND_BLOCK_NUM][CFG_NAND_PLANE_NUM];
u8  blk_ers_cnt[CFG_NAND_CHANNEL_NUM][CFG_NAND_LUN_NUM][CFG_NAND_BLOCK_NUM][CFG_NAND_PLANE_NUM];
u8  blk_bad[CFG_NAND_CHANNEL_NUM][CFG_NAND_LUN_NUM][CFG_NAND_BLOCK_NUM][CFG_NAND_PLANE_NUM];

u8  superblk_bad[CFG_NAND_CHANNEL_NUM][CFG_NAND_LUN_NUM][CFG_NAND_BLOCK_NUM - RESERVED_SUPERBLK];
u8  superblk_ers_cnt[CFG_NAND_CHANNEL_NUM][CFG_NAND_LUN_NUM][CFG_NAND_BLOCK_NUM - RESERVED_SUPERBLK];

Queue* lba_queue[CFG_NAND_CHANNEL_NUM] = {NULL};
int switch_cnt = 0;

int alloc_channel(int chl_id);
int free_channel(int chl_id);

void set_chl_allocated(int chl_id);
void set_chl_free(int chl_id);

int erase_all_blk_chl(int chl_id);
int is_allocated(int chl_id);

u32 get_free_blk(int chl_id);
int get_free_blk_size(int chl_id);

int read_page(u32 lba, int page_id, char *buf, char *metabuf);
int read_data_ppa(int devid, u32 nsid, u32 ppa_addr, u16 qid, int nlb, char *buf, char *metabuf);
int read_data(u32 lba, int page_id, char *buf, char *metabuf, int page_num);

int write_page(u32 lba, int page_id, char *buf, char *metabuf);
int write_data(u32 lba, int page_id, char *buf, char *metabuf, int page_num);
int write_data_ppa(int devid, u32 nsid, u32 ppa_addr, u16 qid, int nlb, char *buf, char *metabuf);

#endif
