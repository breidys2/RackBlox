/********************************************************************
* FILE NAME: parser.c
*
*
* PURPOSE: parsing for each cmd name.
*
* 
* NOTES:
*
* 
* DEVELOPMENT HISTORY: 
* 
* Date Author Release  Description Of Change 
* ---- ----- ---------------------------- 
*2014.6.27, dxu, initial coding.
*
********************************************************************/ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include "nexus.h"

/*******************************************************************
*
*FUNCTION DESCRIPTION: display function
* 
*FUNCTION NAME:pr
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
void pr(void *adrs, int n, int width )
{
    int i;
    void *tmpadrs = (void *)((u64) adrs & ~(width - 1));
    u8 *pbyte;        
    u16 *pshort;    
    u32 *pint;    
    u64 *plong;        

    /* print column sign */
    printf("        ");
    switch (width)
    {
        case 1:
            for(i = 0; i < LINE_BYTES; i+=width)
            {
                printf("%-2x ", i);
            }
            break;
        case 2:
            for(i = 0; i < LINE_BYTES; i+=width)
            {
                printf("%-4x ", i);
            }
            break;
        case 4:
            for(i = 0; i < LINE_BYTES; i+=width)
            {
                printf("%-8x ", i);
            }
            break;
        case 8:
            for(i = 0; i < LINE_BYTES; i+=width)
            {
                printf("%-16x ", i);
            }
            break;

        default:
            for(i = 0; i < LINE_BYTES; i+=width)
            {
                printf("%-2x ", i);
            }
            break;
            
    }
    printf("\n---------");
    for(i = 0; i < LINE_BYTES; i++)
    {
        printf("---");
    }

    /* print out */
    while (n-- > 0)
    {
        if (LINE_BYTES/width == i) {
            i = 0;
            printf ("\n0x%04x: ", (u16)((u64) tmpadrs - (u64)adrs));
        }

    switch (width)
    {
        case 1:
            pbyte = (u8*)tmpadrs;
            printf ("%02x ", *pbyte);
            break;
        case 2:
            pshort = (u16*)tmpadrs;
            printf ("%04x ", *pshort);
            break;
        case 4:
            pint = (u32*)tmpadrs;
            printf ("%08x ", *pint);
            break;
        case 8:
            plong = (u64*)tmpadrs;
            printf ("%08lx%08lx ", *(plong+1), *plong);
            break;
        
        default:
            pbyte = (u8 *)tmpadrs;
            printf ("%02x ", *pbyte);
            break;
        }

        tmpadrs = (void *)((u64)tmpadrs + width);
        i++;
    }

    printf ("\n");
}

/*******************************************************************
*
*******************************************************************/
void print_format(void *adrs, int n, int type )   //type :  0-cq      1-sq
{
    int i=0,j=0;
    void *tmpadrs = adrs; 
    u8 *pbyte;
    u32 line = 0;

    /* print column sign */
    printf("        ");

    if(type == 1)           //printf CQ
    {
        printf("result     |");        
        printf("rsvd       |");        
        printf("sq_hd|");        
        printf("sq_id|");        
        printf("cmdid|");
        printf("status");
    } 
    else if(type == 0)       //printf SQ
    {
        printf("op flg cid |");
        printf("nsid       |");        
        printf("rsvd       |");
        printf("rsvd       |");
        printf("metadata    ");        
        printf("           |");
        printf("prp1        ");         
        printf("           |");
        printf("prp2        ");        
        printf("           |");
        printf("Dword10    |");
        printf("Dword11    |");        
        printf("Dword12    |");
        printf("Dword13    |");
        printf("Dword14    |");        
        printf("Dword15    |");
    }
            
    printf("\n---------");

    if(type == 1){
        for(i = 0; i < CQE_SIZE; i++)
        {
            printf("---");
        }
    }
    else if(type ==0) {
        for(i = 0; i < SQE_SIZE; i++)
        {
            printf("---");
        }
    }

    /* print out */
    while (n-- > 0)
    {
        if(type == 1 ) {                //CQ
            if (CQE_SIZE == i) {
                i = 0;
                printf ("\n%06d: ", line++);
            }
        }
        else if(type == 0) {            //SQ
            if (SQE_SIZE == i) {
                i = 0;
                printf ("\n%06d: ", line++);
            }            
        }
        
        pbyte = (u8*)tmpadrs;        
        
        if(++j==4){
            printf ("%02x|", *pbyte);
            j=0;
        }
        else{
            printf ("%02x ", *pbyte);   
        }
        tmpadrs = tmpadrs + 1; 
        i++;
    }

    printf ("\n");
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: write to the assigned file, print if the filename is null
* 
*FUNCTION NAME:parse_write_file
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_write_file(char* filename, void* memaddr, u32 length, int width)
{
    int fd = ERROR;
    int ret = 0;

    if(0 == strlen(filename)){
        pr(memaddr, length, width);
    } else {
        /* creating file */
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
        if (0 > fd) {
            perror("open fd");
            return ERROR;
        }

        /* write file */
        if (0 > write(fd, memaddr, length)) {
            perror("write fd");
            ret = ERROR;
            goto close_fd;
        }
    }
    
close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: read from the assigned file, return a ptr to the data of the file
* 
*FUNCTION NAME:parse_read_file
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
void* parse_read_file(char* filename, u32 *file_len)
{
    int fd = ERROR;
    struct stat filestat;
    void* memaddr = NULL;
    u32 MAX_SIZE = PAGE_SIZE*1024;

    /*get file length, now for data within 4k, larger data is under verifing and to be continued... */
    if ((0 > stat(filename, &filestat) || (MAX_SIZE < filestat.st_size))) {
        perror("stat");
        return (void*)ERROR;
    } else {
        *file_len = filestat.st_size;
    }

    memaddr = malloc(*file_len);
    if (NULL == memaddr) {
        perror("malloc");
        return (void*)ERROR;
    }
    
    /* open file */
    fd = open(filename, O_RDONLY);
    if (0 > fd) {
        perror("open fd");
        free(memaddr);
        return (void*)ERROR;
    }

    /* read file */
    if (*file_len != read(fd, memaddr, *file_len)) {
        perror("read fd");
        free(memaddr);
        close(fd);
        return (void*)ERROR;
    }
    
    close(fd);
    return memaddr;
}

// return 0: success
int ersppa_sync(int devid, u32 nsid, u32 ppa_addr, u16 qid, u16 nlb)
{
    int result = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    char name[DEV_NAME_SIZE];
    struct nvme_ppa_command cmd_para; 
    
    memset(&cmd_para, 0, sizeof(cmd_para));
    cmd_para.opcode = nvme_cmd_ersppa;
    cmd_para.nsid = nsid;
    cmd_para.apptag = qid;
    cmd_para.nlb = nlb;// either 0 for 1 PPA, or 15 for one PPA_list for all 16x CH, or 63 for one PPA_list for all 16x CH and all planes (2x or 4x)
    cmd_para.start_list = ppa_addr;
    
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, devid);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        printf("open %s fail", name);
        return 1;
    }
    if (ioctl(fd, cmd, &cmd_para) < 0) {
        printf("ioctl %s error\n", name);
        result = 1;
        goto close_fd;
    }
close_fd:
    close(fd);
    return result;  
}

void set_ppa_addr(struct physical_address *ppa, 
                             u8 ch, u8 sec, u8 pl, u8 lun, u8 sb, u16 wl, u16 blk) 
{
    ppa->ppa = 0; // make sure ppa is cleared including ->nand.resved
    ppa->nand.ch = ch;
    ppa->nand.sec = sec;
    ppa->nand.pl = pl;
    ppa->nand.lun = lun;
    #ifdef FLASH_SKHYNIX
    ppa->nand.sb = sb;
    #endif
    ppa->nand.pg = wl;
    ppa->nand.blk = blk;
}

int skip_maskchannel(u32 ppa_addr, u16 channel_mask)
{
    int status = GOOD_PPA;
    struct physical_address addr;
    u16 ch, mark;
    
    addr.ppa = ppa_addr;
    ch = addr.nand.ch;  
    mark = 1 << ch;
    
    if((mark & channel_mask) != 0){
        status = GOOD_PPA;
    } else {
        status = BAD_PPA;
    }
    return status;
}
int is_perfect_lun(u16 blk, u16 lun, u16 *badbin)
{
    int status = GOOD_PPA;
    u16 *badmark;
  
    /* skip bad block */
    if(badbin){
        badmark = badbin + ((CFG_NAND_BLOCK_NUM - 1) - blk) * LUN_NUM;

        /* to check this lun if all channels are perfect  */
        if(badmark[lun]){
            status = BAD_PPA;
        }else{
            status = GOOD_PPA;
        }        
    }
    return status;
}

int skip_badblk(u32 ppa_addr, u16 *badbin)
{
    u16 ch;
    int status = GOOD_PPA;
    struct physical_address addr;
    u16 *badmark;
    u16 mark, blk, lun;
  
    addr.ppa = ppa_addr;
    ch = addr.nand.ch;  
    blk = addr.nand.blk;
    lun = addr.nand.lun;    
    mark = 1 << ch;
    /* skip bad block */
    if(badbin){
        badmark = badbin + ((CFG_NAND_BLOCK_NUM - 1) - blk) * LUN_NUM;
        
        /* to check this channel if bad block */
        if(badmark[lun] & mark){
            status = BAD_PPA;
        }else{
            status = GOOD_PPA;
        }        
    }
    return status;
}


int skip_ppa(u32 ppa_addr, u16 *badbin, u16 channel_mask)
{
    int flag = GOOD_PPA;
    
    flag = skip_badblk(ppa_addr, badbin);
    if (flag == BAD_PPA) {
        return flag;
    }
    
    flag = skip_maskchannel(ppa_addr, channel_mask);
    if (flag == BAD_PPA) {
        return flag;
    }

    return flag;
}
// TODO: assume all 16 channels for now, no considering of CHMASK
int erase_raidblock_helper(int devid, u32 nsid, u16 qid, u16 block, u16* badbin, u16 channel_mask)
{
    int flag = GOOD_PPA, perfect_block = GOOD_PPA, perfect_lun;
    int lun_val, ch_val, pl_val;
    u32 ppa_addr = 0;

    PRINT("erasing block: %d/%d\n", block, CFG_NAND_BLOCK_NUM-1);
    if (badbin != NULL) {  
        lun_val = 0 ;
        while ((lun_val < CFG_NAND_LUN_NUM) && perfect_block) {
            ppa_addr = (lun_val << (CH_BITS+EP_BITS+PL_BITS)) | 
                       (block << (CH_BITS+EP_BITS+PL_BITS+LN_BITS+PG_BITS));
            perfect_block = is_perfect_lun(block, lun_val, badbin);
            lun_val++;
        }        
    }
    
    if (badbin == NULL || perfect_block) {
        // there is no badblock info or this is the perfect block, where all LUN/CH are good), we will use ppa_list(64 x LUNs) to speed up, assume channel_mask is 0xffff
        ppa_addr = (block << (CH_BITS+EP_BITS+PL_BITS+LN_BITS+PG_BITS));
        // fast mode, one cmd will cover all luns & all planes on all channels
        ersppa_sync(devid, nsid, ppa_addr, qid, CFG_NAND_LUN_NUM * CFG_NAND_PLANE_NUM * CFG_NAND_CHANNEL_NUM - 1); 
    } else {
        for(lun_val = 0; lun_val < CFG_NAND_LUN_NUM; lun_val++)
        {
            ppa_addr = (lun_val << (CH_BITS+EP_BITS+PL_BITS)) | 
                       (block << (CH_BITS+EP_BITS+PL_BITS+LN_BITS+PG_BITS));
            perfect_lun = is_perfect_lun(block, lun_val, badbin);

            if (perfect_lun) {
                // fast mode, one cmd will cover one lun for all planes on all channels
                PRINT("\t lun: %d/%d\n", lun_val, CFG_NAND_LUN_NUM-1);
                ersppa_sync(devid, nsid, ppa_addr, qid, CFG_NAND_PLANE_NUM * CFG_NAND_CHANNEL_NUM - 1); 
            } else {
                PRINT("\t lun: %d/%d skipping ch: ", lun_val, CFG_NAND_LUN_NUM-1);
                // because ERASE IOCLTL only supports CH++&PL++, we have to send one PPA each time to skip bad blocks
                for(ch_val = 0; ch_val < CFG_NAND_CHANNEL_NUM; ch_val++)
                {                        
                    ppa_addr =   ch_val |                                  
                                 (lun_val << (CH_BITS+EP_BITS+PL_BITS)) |
                                 (block << (CH_BITS+EP_BITS+PL_BITS+LN_BITS+PG_BITS));
                            
                    flag = skip_ppa(ppa_addr, badbin, channel_mask);
                    if (flag == BAD_PPA) {
                        PRINT("%d, ", ch_val);
                        continue;
                    } else {                        
                        for(pl_val = 0; pl_val < CFG_NAND_PLANE_NUM; pl_val++) {
                            ersppa_sync(devid, nsid, ppa_addr | (pl_val << (CH_BITS+EP_BITS)), qid, 0); // slow mode, one PPA each time
                        }
                    }                                       
                }
                PRINT("\n");
            }
        }
    }
    return 0;
}

/* erase one raidblock according input badbin file */
int parse_erase_raidblock_cmdline(int argc, char* argv[])
{
    int result = 0;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:i:q:b:m:f:";
    char filename[MAX_FILE_NAME] = {0};
    int devid = 0;
    u32 nsid = 1; 
    u16 qid = 1;
    u16 chmask_sw = 0xffff; 
    u16 *badbin = NULL;
    u32 file_len;

    u32 channel;
    u16 chmask_hw;
    u16 channel_mask;
    u32 block = 0;   
   

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                devid = (u16)para;
                PRINT("device index: %d.\n", devid);
                break;
                
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                qid = (u16)para;
                PRINT("Queue ID: %d.\n", qid);
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                nsid = (u32)para;
                PRINT("cmd_para.nsid: 0x%x.\n", nsid);
                break;
                
            case 'b':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                block = (u32)para % CFG_NAND_BLOCK_NUM;
                PRINT("raid block: 0x%x.\n", block);
                break;    
            case 'm':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                chmask_sw = (u16)para;
                PRINT("chmask_sw: %d.\n", chmask_sw);
                break;
                  
                
            default :
                printf("unexpected parameter of rstns: %c.\n", opt);
                break;
        }
    }

        
    // get badbin
    if(strlen(filename)!=0){
        badbin = (u16*)parse_read_file(filename, &file_len);
        if (badbin == NULL) {
            printf("read file failed.\n");
            return ERROR;
        }
    
        /* file length is (BL_NUM * LUN_NUM * CH_NUM / 8) bytes */
        if(file_len < CFG_NAND_BLOCK_NUM * CFG_NAND_LUN_NUM * CFG_NAND_CHANNEL_NUM / 8){          
            printf("file length is too short.\n");
            result = -EIO;
            goto free_badbin;
        }
    }
    
    read_nvme_reg32(devid, CHANNEL_COUNT, &channel);
    if(channel == 4) {
        chmask_hw = CHANNEL_MASK_4;
    } else if(channel == 8) {
        chmask_hw = CHANNEL_MASK_8;     
    } else {
        chmask_hw = CHANNEL_MASK_16;
    }
    channel_mask = chmask_sw & chmask_hw;
    
    erase_raidblock_helper(devid, nsid, qid, block, badbin, channel_mask);
    
free_badbin:
    free(badbin);
    return result;
}

/* erase all disk according input badbin file */
int parse_erase_disk_cmdline(int argc, char* argv[])
{
    int result = 0;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:i:q:m:f:";
    char filename[MAX_FILE_NAME] = {0};
    int devid = 0;
    u32 nsid = 1; 
    u16 qid = 1;
    u16 chmask_sw = 0xffff; 
    u16 *badbin = NULL;
    u32 file_len;

    u32 channel;
    u16 chmask_hw;
    u16 channel_mask;
    int blk_val;

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                devid = (u16)para;
                PRINT("device index: %d.\n", devid);
                break;
                
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                qid = (u16)para;
                PRINT("Queue ID: %d.\n", qid);
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                nsid = (u32)para;
                PRINT("cmd_para.nsid: 0x%x.\n", nsid);
                break;
                
            case 'm':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                chmask_sw = (u16)para;
                PRINT("chmask_sw: %d.\n", chmask_sw);
                break;
                  
                
            default :
                printf("unexpected parameter of rstns: %c.\n", opt);
                break;
        }
    }

        
    // get badbin
    if(strlen(filename)!=0){
        badbin = (u16*)parse_read_file(filename, &file_len);
        if (badbin == NULL) {
            printf("read file failed.\n");
            return ERROR;
        }
    
        /* file length is (BL_NUM * LUN_NUM * CH_NUM / 8) bytes */
        if(file_len < CFG_NAND_BLOCK_NUM * CFG_NAND_LUN_NUM * CFG_NAND_CHANNEL_NUM / 8){          
            printf("file length is too short.\n");
            result = -EIO;
            goto free_badbin;
        }
    }
    
    read_nvme_reg32(devid, CHANNEL_COUNT, &channel);
    if(channel == 4) {
        chmask_hw = CHANNEL_MASK_4;
    } else if(channel == 8) {
        chmask_hw = CHANNEL_MASK_8;     
    } else {
        chmask_hw = CHANNEL_MASK_16;
    }
    channel_mask = chmask_sw & chmask_hw;

    for (blk_val = 0; blk_val < CFG_NAND_BLOCK_NUM; blk_val++)
    {
        erase_raidblock_helper(devid, nsid, qid, blk_val, badbin, channel_mask);        
    }   

free_badbin:
    free(badbin);
    return result;
}

/*******************************************************************************
 *offset: register offset
 *regval: return val
 *return: 1 read fail     0 read success
 *******************************************************************************/
int read_nvme_reg32(int devid, u32 offset, u32 *regval)
{
    int result = 0;
    int fd;
    int cmd = NEXUS_IOCTL_RD_REG;
    char name[DEV_NAME_SIZE];
    struct rdmem_stru cmd_para;

    memset(&cmd_para, 0, sizeof(cmd_para));
    cmd_para.length = sizeof(u32);
    cmd_para.mem_addr = offset;   //which register
    cmd_para.pdata = regval;

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, devid);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        printf("open %s fail", name);
        return 1;
    }
    
    if (ioctl(fd, cmd, &cmd_para) < 0) {
        printf("ioctl %s error\n", name);
        result = 1;
        goto close_fd;
    }

close_fd:
    close(fd);
    return result;  
}

int write_nvme_reg32(int devid, u32 mem_addr, u32 regval)
{
    int result = 0;
    int fd;
    int cmd = NEXUS_IOCTL_WR_REG;
    char name[DEV_NAME_SIZE];
    struct wrmem_stru cmd_para;

    memset(&cmd_para, 0, sizeof(cmd_para));
    cmd_para.mem_addr = mem_addr;   //which register
    cmd_para.pdata = &regval;
    cmd_para.length = sizeof(u32);

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, devid);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        printf("open %s fail", name);
        return 1;
    }
    
    if (ioctl(fd, cmd, &cmd_para) < 0) {
        printf("ioctl %s error\n", name);
        result = 1;
        goto close_fd;
    }

close_fd:
    close(fd);
    return result;  
}

void parse_cmd_returnval(int cmd, int retval, u16 devid, u16 qid)
{
    u32 regval;
    u8 sc, sct;

    if(retval < 0) {
        printf("input paramter invalid or cmd cancel (detail refer dmesg info)\n");
        return;
    } else if (retval == 0) {
        //PRINT("cmd execute success\n");
        return;
    }

    //if retval > 0 need to parse status  further
    printf("the cmd return status:0x%x\n", retval);
    sc = retval & 0xff;
    sct = (retval >> 8) &0x7;
    printf("status code:0x%x    status code type:0x%x\n", sc, sct);
    if(sct == 0x00) {
        printf("Generic Command status\n");
    } else if(sct == 0x01) {
        printf("Command Specific status\n");            
    } else if (sct == 0x02) {
        printf("Media Errors\n");
    } else if(sct == 0x07) {
        printf("Vendor Specific\n");
    } else {
        printf("Reserved\n");
    }

    //Read some Register
    read_nvme_reg32(devid, 0x1000+qid*8, &regval); //SQDB
    printf("SQ%dTDB:0x%x\n", qid, regval);
    read_nvme_reg32(devid, 0x1004+qid*8, &regval); //CQDB               
    printf("CQ%dHDB:0x%x\n", qid, regval);
    
    if(retval == 0x40ff) {
        printf("Write PPA Program Fail orErase Fail\n");
    } else if (retval == 0x4281) {
        printf("Read PPA ECC Can not Recovery\n");
    } else if(retval == 0x4004) {
        printf("CRC Error\n");
    } else if(retval == 0x4080) {
        printf("LBA out of range\n");
    } else {
        printf("Unknowd User Error Code\n");
    }
    
    return;
}

u32 rd1dword(int fd, u32 addr)
{
    int cmd = NEXUS_IOCTL_RD_DWORD;
    u32 kdata;
    struct rdmem_stru cmd_para;
    cmd_para.mem_addr = addr;
    cmd_para.length   = sizeof(u32);
    cmd_para.pdata    = &kdata;

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0 rd1dword");
        return  ERROR;
    }
    return kdata;
}

int wr1dword(int fd, u32 addr, u32 data)
{
    int cmd = NEXUS_IOCTL_WR_DWORD;
    struct wrmem_stru cmd_para;
    u32 val = data;
    cmd_para.pdata  = &val;
    cmd_para.mem_addr = addr;
    cmd_para.length   = sizeof(u32);
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0 wr1dword");
        return ERROR;
    }
    return 0;
}
int bwrdword(int fd, u32 addr, u32 data)
{
    int cmd = NEXUS_IOCTL_BWR_DWORD;
    struct wrmem_stru cmd_para;
    u32 val = data;
    cmd_para.pdata  = &val;
    cmd_para.mem_addr = addr;
    cmd_para.length   = sizeof(u32);
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus bwrdword");
        return ERROR;
    }
    return 0;
}
void brddword(int fd, u32 addr, u32 a[BURST_DWORD_NUM])
{
    int cmd = NEXUS_IOCTL_BRD_DWORD;
    u32 kdata [BURST_DWORD_NUM];
    struct rdmem_stru cmd_para;
    int i;
    cmd_para.mem_addr = addr;
    cmd_para.length   = sizeof(u32) ;
    cmd_para.pdata    = kdata;

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus");
    }
    for(i = 0; i < BURST_DWORD_NUM; i ++ )
    { 
        a[i] = kdata [i];
    }
}

u32 tran_data ( int n )
{
    u32 a = DDR_DATA_BUS;
    u32 b = 1;
    u32 c;
    c = a - (b << n);
    return c;
}
u32 tran_addr ( int n )
{
    u32 a = DDR_ADDR_DWORD;      // dword, 0 and 1 bit is 0
    u32 b = 1;
    u32 c;
    c = a - (b << n);
    return c;
}

void analyze_pci_command(u16 pci_command)
{
    if ((pci_command & 0x1) == 1) {
        printf("\tIO Space Enable");
    } else {
        printf("\tIO Space Disable");
    }

    if ((pci_command & 0x2) == 0x2) {
        printf("   Memory Space Enable");
    } else {
        printf("   Memory Space Disable");
    }
    
    if ((pci_command & 0x4) == 0x4) {
        printf("   Bus Master Enable\n");
    } else {
        printf("   Bus Master Disable\n");
    }
}

void analyze_pci_status(u16 pci_status)
{
    if ((pci_status & 0x8000) == 0x8000) {
        printf("\tDetect Parity Error\n");
    } else {
    }

    if ((pci_status & 0x2000) == 0x2000) {
        printf("\tReceived Master-Abort\n");
    } else {
    }
    
    if ((pci_status & 0x1000) == 0x1000) {
        printf("\tReceived Target-Abort\n");
    } else {
    }    
}

void analyze_pci_cc(u32 pci_class_code)
{
    u8 bcc, scc, pi;
    
    pi = pci_class_code & 0xff;
    scc = (pci_class_code >> 8) & 0xff;
    bcc = (pci_class_code >> 16) & 0xff;
    printf("\tProgramming Interface:0x%x  Sub Class code:0x%x, Base Class Code:0x%x\n", pi, scc, bcc);
    
}
void analyze_nvme_cap(u64 nvme_cap)
{
    u16 mqes;
    mqes = nvme_cap & 0xffff;
    printf("\tMaximum IO Queue Entries Support %d(0-base)\n", mqes);
    if ((nvme_cap & 0x10000) == 0x10000) {
        printf("\tIO Queue Should Physically Contiguous\n");
    } else {
        printf("\tController Supports IO Queue not Physically Contiguous\n");
    }
}
int parse_rdreg_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_RD_REG;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "r:k:";
    struct rdmem_stru cmd_para;
    u32 data;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'r':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.mem_addr = (u32)para;
                PRINT("register offset: 0x%x.\n", cmd_para.mem_addr);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;

            default :
                printf("unexpected parameter of rdreg: %c.\n", opt);
                break;
        }
    }

    cmd_para.pdata = &data;
    cmd_para.length   = sizeof(u32);

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    printf("reg(0x%x): 0x%08x\n", cmd_para.mem_addr, data);
    
close_fd:
    close(fd);
    return ret;
}

int parse_wrreg_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_WR_REG;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "r:v:k:";
    struct wrmem_stru cmd_para;
    u32 data;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'r':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.mem_addr = (u32)para;
                PRINT("register offset: 0x%x.\n", cmd_para.mem_addr);
                break;
                
            case 'v':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                data = (u32)para;
                cmd_para.pdata  = &data;
                PRINT("register value to write: 0x%x.\n", *cmd_para.pdata);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of wrreg: %c.\n", opt);
                break;
        }
    }

    cmd_para.length   = sizeof(u32);

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}
int parse_rdreg64_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_RD_REG64;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "r:k:";
    struct rdreg64_stru cmd_para;
    u64 data;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'r':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.mem_addr = (u32)para;
                PRINT("register offset: 0x%x.\n", cmd_para.mem_addr);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            
            default :
                printf("unexpected parameter of rdreg64: %c.\n", opt);
                break;
        }
    }
    cmd_para.pdata = &data;

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    printf("reg(0x%x): 0x%lx\n", cmd_para.mem_addr, data);
    
close_fd:
    close(fd);
    return ret;
}

int parse_wrreg64_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    long long para1;
    int fd = 0;
    int cmd = NEXUS_IOCTL_WR_REG64;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "r:v:k:";
    struct wrreg64_stru cmd_para;
    u64 data;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'r':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.mem_addr = (u32)para;
                PRINT("register offset: 0x%x.\n", cmd_para.mem_addr);
                break;
                
            case 'v':
                para_num++; 
                para1 = strtoll(optarg, &endptr, 0);
                data = (u64)para1;
                cmd_para.pdata = &data;
                PRINT("register value to write: 0x%lx.\n", *cmd_para.pdata);
                break;
                
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of wrreg64: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
close_fd:
    close(fd);
    return ret;
}

int parse_dumpnexus_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_DUMPNVME;
    long para_num = 0;
    int opt = 0;
    char* endptr;
    char* optstring = "k:";
    struct nexus_dumpinfo cmd_para;
    struct nvme_bar_space *nvme_reg_stru = NULL;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    int i;

    memset(&cmd_para, 0, sizeof(cmd_para));
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt)
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }
    nvme_reg_stru = (struct nvme_bar_space *)malloc(sizeof(struct nvme_bar_space));
    if (nvme_reg_stru == NULL) {
        return -1;
    }
    cmd_para.rd_mem.pdata = (u32 *)nvme_reg_stru;
    cmd_para.rd_mem.mem_addr = 0;
    cmd_para.rd_mem.length   = sizeof(struct nvme_reg);

    cmd_para.nsid_active = (u32 *)malloc(PAGE_SIZE);
    if (cmd_para.nsid_active == NULL) {
        free(cmd_para.rd_mem.pdata);
        return -1;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    printf("CAP(Controller Capabilities): 0x%lx\n", nvme_reg_stru->nvmereg.cap);
    analyze_nvme_cap(nvme_reg_stru->nvmereg.cap);
    printf("VS(Version):                  0x%x\n", nvme_reg_stru->nvmereg.vs);
    printf("INTMS(Interrupt Mask Set):    0x%x\n", nvme_reg_stru->nvmereg.intms);
    printf("INTMC(Interrupt Mask Clear):  0x%x\n", nvme_reg_stru->nvmereg.intmc);
    printf("CC(Controller Configuration): 0x%x\n", nvme_reg_stru->nvmereg.cc);
    printf("CSTS(Controller Status):      0x%x\n", nvme_reg_stru->nvmereg.csts);
    printf("AQA(Admin Queue Attributes):  0x%x\n", nvme_reg_stru->nvmereg.aqa);
    printf("ASQ(Admin SQ Base Address):   0x%lx\n", nvme_reg_stru->nvmereg.asq);
    printf("ACQ(Admin CQ Base Address):   0x%lx\n", nvme_reg_stru->nvmereg.acq);     
    printf("Start BAR Address of DRAM:    0x%lx\n", nvme_reg_stru->nvmereg.sbar);
    printf("End BAR Address of DRAM:      0x%lx\n", nvme_reg_stru->nvmereg.ebar);
    for(i=0; i<1024; i++){
        if(cmd_para.nsid_active[i] != 0){
            printf("the %dth namespace id is:      0x%x\n", i, cmd_para.nsid_active[i]);
        }else{
            break;
        }
    }

    printf("Active queue count:           %d\n", cmd_para.queue_cnt);
    printf("All active queue identifier:  ");
    for(i=0; i<cmd_para.queue_cnt; i++){
        printf("%d ", cmd_para.queue[i]);
    }
    printf("\n");
    
    printf("--------------------------------------\n");
    printf("SQ0 Tail DB:                  0x%x\n", nvme_reg_stru->nvmereg.sq0_tdb);
    printf("CQ0 Head DB:                  0x%x\n", nvme_reg_stru->nvmereg.cq0_hdb);    
    printf("--------------------------------------\n");
    printf("SQ1 Tail DB:                  0x%x\n", nvme_reg_stru->nvmereg.sq1_tdb);
    printf("CQ1 Head DB:                  0x%x\n", nvme_reg_stru->nvmereg.cq1_hdb);
    printf("--------------------------------------\n");
    printf("SQ2 Tail DB:                  0x%x\n", nvme_reg_stru->nvmereg.sq2_tdb);
    printf("CQ2 Head DB:                  0x%x\n", nvme_reg_stru->nvmereg.cq2_hdb);    
    printf("--------------------------------------\n");
    printf("SQ3 Tail DB:                  0x%x\n", nvme_reg_stru->nvmereg.sq3_tdb);
    printf("CQ3 Head DB:                  0x%x\n", nvme_reg_stru->nvmereg.cq3_hdb);    
    printf("--------------------------------------\n");    
    printf("SQ0 Fetch Count:              0x%x\n", nvme_reg_stru->sqfc0);
    printf("SQ0 completion count:         0x%x\n", nvme_reg_stru->sqcc0);    
    printf("--------------------------------------\n");
    printf("SQ1 Fetch Count:              0x%x\n", nvme_reg_stru->sqfc1);
    printf("SQ1 completion count:         0x%x\n", nvme_reg_stru->sqcc1);    
    printf("--------------------------------------\n");
    printf("SQ2 Fetch Count:              0x%x\n", nvme_reg_stru->sqfc2);
    printf("SQ2 completion count:         0x%x\n", nvme_reg_stru->sqcc2);    
    printf("--------------------------------------\n");
    printf("SQ3 Fetch Count:              0x%x\n", nvme_reg_stru->sqfc3);
    printf("SQ3 completion count:         0x%x\n", nvme_reg_stru->sqcc3);
close_fd:
    close(fd);
free:
    free(nvme_reg_stru);
    return ret;
}
int parse_dumppci_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_DUMPPCI;
    long para_num = 0;
    int opt = 0;
    char* endptr;
    char* optstring = "k:";
    struct pci_header *cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }
    
    cmd_para = malloc(sizeof(struct pci_header)); 
    if(cmd_para == NULL) {
        return ERROR;
    }
    memset(cmd_para, 0, sizeof(struct pci_header));

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free;
    }
    if (0 > ioctl(fd, cmd, cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    printf("Nexus VendorID              =0x%x\n", cmd_para->pci_vendor_id);
    printf("Nexus DeviceID              =0x%x\n", cmd_para->pci_device_id);
    printf("Nexus Command Register      =0x%x\n", cmd_para->pci_command);
    analyze_pci_command(cmd_para->pci_command);
    printf("Nexus Device STATUS         =0x%x\n", cmd_para->pci_status);
    analyze_pci_status(cmd_para->pci_status);
    printf("Nexus Revision ID           =0x%x\n", cmd_para->pci_rev_id);   
    printf("Nexus PCI Class Code        =0x%x\n", cmd_para->pci_class_code); 
    analyze_pci_cc(cmd_para->pci_class_code);
    printf("Nexus Cache Line Size       =0x%x\n", cmd_para->pci_cache_line_size);    
    printf("Nexus Master Latency Timer  =0x%x\n", cmd_para->pci_lat_tim);    
    printf("Nexus Header Type           =0x%x\n", cmd_para->pci_header_t);    
    printf("Nexus Bulid in self test    =0x%x\n", cmd_para->pci_bist);    
    printf("Nexus CardBus CIS Pointer   =0x%x\n", cmd_para->ccptr);       
    printf("Nexus Subsystem VendorID    =0x%x\n", cmd_para->pci_sub_vid);    
    printf("Nexus Subsystem ID          =0x%x\n", cmd_para->pci_sub_id);   
    printf("Nexus Rom base address      =0x%x\n", cmd_para->pci_base_rom);    
    printf("Nexus Capabilities Pointer  =0x%x\n", cmd_para->pci_cap);    
    printf("Nexus Interrupt Line        =0x%x\n", cmd_para->pci_irq_line);    
    printf("Nexus Interrupt Pin         =0x%x\n", cmd_para->pci_irq_pin);    
close_fd:
    close(fd);
free:
    free(cmd_para);
    return ret;
}

int parse_checkcq_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_CHECK_CQ;
    long para_num = 0;
    int opt = 0;
    long para = 0;
    char* endptr;
    char* optstring = "q:k:";
    struct nvme_read_cq cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    memset(&cmd_para, 0, sizeof(cmd_para));
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qid= (u32)para;             /*note qid which IO queue to run this command*/
                PRINT("qid: %d.\n", cmd_para.qid);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }
    cmd_para.pdata = malloc(MAX_Q_DEPTH * CQE_SIZE);
    if (cmd_para.pdata == NULL) {
        return ERROR;
    }
    memset(cmd_para.pdata, 0, MAX_Q_DEPTH * CQE_SIZE);
    
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    print_format(cmd_para.pdata, cmd_para.length, 1);
close_fd:
    close(fd);
free_mem:
    free(cmd_para.pdata);
    return ret;
}
int parse_checksq_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_CHECK_SQ;
    long para_num = 0;
    int opt = 0;
    long para = 0;
    char* endptr;
    char* optstring = "q:k:";
    struct nvme_read_cq cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    memset(&cmd_para, 0, sizeof(cmd_para));
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qid= (u32)para;             /*note qid which IO queue to run this command*/
                PRINT("qid: %d.\n", cmd_para.qid);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }
    cmd_para.pdata = malloc(MAX_Q_DEPTH * SQE_SIZE);
    if (cmd_para.pdata == NULL) {
        return ERROR;
    }
    memset(cmd_para.pdata, 0, MAX_Q_DEPTH * CQE_SIZE);

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    print_format(cmd_para.pdata, cmd_para.length, 0);
close_fd:
    close(fd);
free_mem:
    free(cmd_para.pdata);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-ppawrsync", and excecute this function
* 
*FUNCTION NAME:parse_ppawr_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_wrppa_sync_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:n:f:m:i:c:q:k:t:";
    struct nvme_ppa_command cmd_para;
    char filename1[MAX_FILE_NAME] = {0};
    char filename2[MAX_FILE_NAME] = {0};    
    char name[DEV_NAME_SIZE];
    u32 length;
    u16 instance = 0;    

    memset(&cmd_para, 0 , sizeof(cmd_para));
    cmd_para.apptag = 1;  //default qid
    cmd_para.nsid = 1;    //default nsid
    cmd_para.appmask = ADDR_FIELDS_SHIFT_EP; // default EP mode
    cmd_para.control = NVME_SINGLE_PLANE;
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {   
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_list = (u64)para;
                PRINT("start_addr: 0x%llx.\n", cmd_para.start_list);
                break;

            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nlb = (u16)para - 1;
                PRINT("nlb: %d.\n", cmd_para.nlb);                
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;
    
            case 'c':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                if (para == 0) cmd_para.appmask = ADDR_FIELDS_SHIFT_CH;     // if CH mode, we need at least 64 PPAs for 1 plane on all 16 CH
                PRINT("addr_field_shift: %d.\n", cmd_para.appmask);
                break;

            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.apptag = (u16)para;     /*note qid which IO queue to run PPA command*/
                PRINT("qid: %d.\n", cmd_para.apptag);
                break;                

            case 'f':
                para_num++;
                strncpy(filename1, optarg, sizeof(filename1)-1);
                PRINT("filename1: %s.\n", filename1);
                break;
                
            case 'm':
                para_num++;
                strncpy(filename2, optarg, sizeof(filename2)-1);
                PRINT("filename2: %s.\n", filename2);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                instance = (u16)para;
                PRINT("device index: %d.\n", instance);
                break;

            case 't':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.control = (u16)para;
                PRINT("control: %d.\n", cmd_para.control);
                break;
                
            default :
                printf("unexpected parameter of ppawr: %c.\n", opt);
                break;
        }
    }
    if (cmd_para.appmask == ADDR_FIELDS_SHIFT_CH) { 
        cmd_para.control = NVME_SINGLE_PLANE; // CH mode, we must be SINGLE_PLANE due to PPA list size limited to 64
    } else if ((cmd_para.nlb + 1) > CFG_NAND_PLANE_NUM * CFG_DRIVE_EP_NUM) {
        cmd_para.control = NVME_QUART_PLANE;  // EP mode, we have PPA for more than one plane
    }

    cmd_para.opcode = nvme_cmd_wrppa;
    
    cmd_para.prp1 = (s64)parse_read_file(filename1, &length);
    if ((s64)ERROR == cmd_para.prp1) {
        printf("read file failed.\n");
        return ERROR;
    }

    cmd_para.metadata= (s64)parse_read_file(filename2, &length);
    if ((s64)ERROR == cmd_para.metadata) {
        printf("read file failed.\n");
        free((void*)cmd_para.prp1);
        return ERROR;
    }
    
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_ptr;
    }
    
    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, cmd_para.apptag);

    close(fd);
free_ptr:
    free((void*)cmd_para.prp1);
    free((void*)cmd_para.metadata);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-ppawrsync", and excecute this function
* 
*FUNCTION NAME:parse_ppawr_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_wrpparaw_sync_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:n:f:m:i:c:q:k:t:";
    struct nvme_ppa_command cmd_para;
    char filename1[MAX_FILE_NAME] = {0};
    char filename2[MAX_FILE_NAME] = {0};    
    char name[DEV_NAME_SIZE];
    u32 length;
    u16 instance = 0;

    memset(&cmd_para, 0 , sizeof(cmd_para));
    cmd_para.apptag = 1;  //default
    cmd_para.nsid = 1;  //default
    cmd_para.appmask = ADDR_FIELDS_SHIFT_EP; // default EP mode
    cmd_para.control = NVME_SINGLE_PLANE;

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {   
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_list = (u64)para;
                PRINT("start_addr: 0x%llx.\n", cmd_para.start_list);
                break;

            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nlb = (u16)para - 1;
                PRINT("nlb: %d.\n", cmd_para.nlb);                
                
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;

            case 'c':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                if (para == 0) cmd_para.appmask = ADDR_FIELDS_SHIFT_CH;     // if CH mode, we need at least 64 PPAs for 1 plane on all 16 CH
                PRINT("addr_field_shift: %d.\n", cmd_para.appmask);
                break;
                
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.apptag = (u16)para;     /*note qid which IO queue to run PPA command*/
                PRINT("qid: %d.\n", cmd_para.apptag);
                break;

            case 'f':
                para_num++;
                strncpy(filename1, optarg, sizeof(filename1)-1);
                PRINT("filename1: %s.\n", filename1);
                break;
                
            case 'm':
                para_num++;
                strncpy(filename2, optarg, sizeof(filename2)-1);
                PRINT("filename2: %s.\n", filename2);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                instance = (u16)para;
                PRINT("device index: %d.\n", instance);
                break;

            case 't':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.control = (u16)para;
                PRINT("control: %d.\n", cmd_para.control);
                break;
                
            default :
                printf("unexpected parameter of ppawr: %c.\n", opt);
                break;
        }
    }

    if (cmd_para.appmask == ADDR_FIELDS_SHIFT_CH) { 
        cmd_para.control = NVME_SINGLE_PLANE; // CH mode, we must be SINGLE_PLANE due to PPA list size limited to 64
    } else if ((cmd_para.nlb + 1) > CFG_NAND_PLANE_NUM * CFG_DRIVE_EP_NUM) {
        cmd_para.control = NVME_QUART_PLANE;  // EP mode, we have PPA for more than one plane
    }

    cmd_para.opcode = nvme_cmd_wrpparaw;
    
    cmd_para.prp1 = (s64)parse_read_file(filename1, &length);
    if ((s64)ERROR == cmd_para.prp1) {
        printf("read file failed.\n");
        return ERROR;
    }

    cmd_para.metadata= (s64)parse_read_file(filename2, &length);
    if ((s64)ERROR == cmd_para.metadata) {
        printf("read file failed.\n");
        free((void*)cmd_para.prp1);
        return ERROR;
    }
    
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_ptr;
    }
    
    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, cmd_para.apptag);

    close(fd);
free_ptr:
    free((void*)cmd_para.prp1);
    free((void*)cmd_para.metadata);
    return ret;
}


/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-ppardsync", and excecute this function
* 
*FUNCTION NAME:parse_ppard_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_rdppa_sync_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:n:i:f:m:c:q:k:t:";
    struct nvme_ppa_command cmd_para;
    char filename1[MAX_FILE_NAME] = {0};
    char filename2[MAX_FILE_NAME] = {0};    
    char name[DEV_NAME_SIZE];
    u16 instance = 0;

    memset(&cmd_para, 0 , sizeof(cmd_para));
    cmd_para.apptag = 1;  //default
    cmd_para.nsid = 1;  //default
    cmd_para.appmask = ADDR_FIELDS_SHIFT_EP; // default EP mode

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {   
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_list = (u64)para;
                PRINT("start_addr: 0x%llx.\n", cmd_para.start_list);
                break;

            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nlb = (u16)para - 1;
                PRINT("nlb: %d.\n", cmd_para.nlb);
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;
            case 'c':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                if (para == 0) cmd_para.appmask = ADDR_FIELDS_SHIFT_CH;     // if CH mode, we need at least 64 PPAs for 1 plane on all 16 CH
                PRINT("addr_field_shift: %d.\n", cmd_para.appmask);
                break;
                
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.apptag = (u16)para;          /*note qid which IO queue to run PPA command*/
                PRINT("qid: %d.\n", cmd_para.apptag);
                break;

            case 'f':
                para_num++;
                strncpy(filename1, optarg, sizeof(filename1)-1);
                PRINT("filename1: %s.\n", filename1);
                break;

            case 'm':
                para_num++;
                strncpy(filename2, optarg, sizeof(filename2)-1);
                PRINT("filename2: %s.\n", filename2);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                instance = (u16)para;
                PRINT("device index: %d.\n", instance);
                break;

            case 't':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.control = (u16)para;
                PRINT("control: %d.\n", cmd_para.control);
                break;

            default :
                printf("unexpected parameter of ppard: %c.\n", opt);
                break;
        }
    }
    
    cmd_para.opcode = nvme_cmd_rdppa;

    cmd_para.prp1 = (s64)malloc(PAGE_SIZE*(cmd_para.nlb+1));
    if (NULL == (void*)cmd_para.prp1) {
        perror("malloc");
        return ERROR;
    }

    cmd_para.metadata= (s64)malloc(META_SIZE*(cmd_para.nlb+1));
    if (NULL == (void*)cmd_para.metadata) {
        free((void*)cmd_para.prp1);
        perror("malloc");
        return ERROR;
    }
  
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }

    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, cmd_para.apptag);
        
    if (OK != parse_write_file(filename1, (void *)cmd_para.prp1, PAGE_SIZE*(cmd_para.nlb+1), 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }

    if (OK != parse_write_file(filename2, (void *)cmd_para.metadata, META_SIZE*(cmd_para.nlb+1), 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }
    
close_fd:
    close(fd);
free_mem:
    free((void*)cmd_para.metadata);
    free((void*)cmd_para.prp1);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-ppardsync", and excecute this function
* 
*FUNCTION NAME:parse_ppard_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_rdpparaw_sync_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:n:i:f:m:q:k:t:";
    struct nvme_ppa_command cmd_para;
    char filename1[MAX_FILE_NAME] = {0};
    char filename2[MAX_FILE_NAME] = {0};    
    char name[DEV_NAME_SIZE];
    u16 instance = 0;
    u32 metaraw_size = 304;

    memset(&cmd_para, 0 , sizeof(cmd_para));
    cmd_para.apptag = 1;  //default
    cmd_para.nsid = 1;  //default

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {   
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_list = (u64)para;
                PRINT("start_addr: 0x%llx.\n", cmd_para.start_list);
                break;

            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nlb = (u16)para - 1;
                PRINT("nlb: %d.\n", cmd_para.nlb);
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;
    
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.apptag = (u16)para;          /*note qid which IO queue to run PPA command*/
                PRINT("qid: %d.\n", cmd_para.apptag);
                break;

            case 'f':
                para_num++;
                strncpy(filename1, optarg, sizeof(filename1)-1);
                PRINT("filename1: %s.\n", filename1);
                break;

            case 'm':
                para_num++;
                strncpy(filename2, optarg, sizeof(filename2)-1);
                PRINT("filename2: %s.\n", filename2);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                instance = (u16)para;
                PRINT("device index: %d.\n", instance);
                break;

            case 't':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.control = (u16)para;
                PRINT("control: %d.\n", cmd_para.control);
                break;

            default :
                printf("unexpected parameter of ppard: %c.\n", opt);
                break;
        }
    }
    
    read_nvme_reg32(instance, MTDSZ, &metaraw_size);
    cmd_para.opcode = nvme_cmd_rdpparaw;

    cmd_para.prp1 = (s64)malloc(PAGE_SIZE*(cmd_para.nlb+1));
    if (NULL == (void*)cmd_para.prp1) {
        perror("malloc");
        return ERROR;
    }

    cmd_para.metadata= (s64)malloc(metaraw_size*(cmd_para.nlb+1));
    if (NULL == (void*)cmd_para.metadata) {
        free((void*)cmd_para.prp1);
        perror("malloc");
        return ERROR;
    }
  
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }

    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, cmd_para.apptag);
        
    if (OK != parse_write_file(filename1, (void *)cmd_para.prp1, PAGE_SIZE*(cmd_para.nlb+1), 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }

    if (OK != parse_write_file(filename2, (void *)cmd_para.metadata, metaraw_size*(cmd_para.nlb+1), 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }
    
close_fd:
    close(fd);
free_mem:
    free((void*)cmd_para.metadata);
    free((void*)cmd_para.prp1);
    return ret;
}


/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-ppawr", and excecute this function
* 
*FUNCTION NAME:parse_ppawrraw_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_ersppa_sync_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:i:q:n:k:t:";
    struct nvme_ppa_command cmd_para;    
    char name[DEV_NAME_SIZE];
    u16 instance = 0;
  
    memset(&cmd_para, 0 , sizeof(cmd_para));
    cmd_para.apptag = 1;  //default
    cmd_para.nsid = 1;  //default

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_list = (u64)para;
                PRINT("ppa_addr: 0x%llx\n", cmd_para.start_list);
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid : %d\n", cmd_para.nsid);
                break;
                
            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nlb = (u16)para-1;                
                PRINT("nlb: %d  0-base\n", cmd_para.nlb);
                break;   
                
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.apptag = (u16)para;         
                PRINT("qid: %d\n", cmd_para.apptag);
                break;

            case 'k':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                instance = (u16)para;
                PRINT("device index: %d.\n", instance);
                break;

            case 't':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.control = (u16)para;
                PRINT("control: %d.\n", cmd_para.control);
                break;

            default :
                printf("unexpected parameter of ersppasync: %c.\n", opt);
                break;
        }
    }

    cmd_para.opcode = nvme_cmd_ersppa;   
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
       perror("open nexus0");
       return ERROR;
    }

    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, cmd_para.apptag);

    close(fd);

    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-fmtnvm", and excecute this function
* 
*FUNCTION NAME:parse_fmtnvm_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_fmtnvm_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_FMT_NVM;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "i:v:k:";
    struct nvme_format_cmd cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;

            case 'v':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.cdw10 = (u32)para;
                PRINT("dword10: %d.\n", cmd_para.cdw10);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of fmtnvm: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}


/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-delsq", and excecute this function
* 
*FUNCTION NAME:parse_delsq_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_delsq_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_DEL_SQ;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "q:k:";
    struct nvme_delete_queue cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qid = (u16)para;
                PRINT("delsq.qid: %d.\n", cmd_para.qid);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of delsq: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-crtsq", and excecute this function
* 
*FUNCTION NAME:parse_crtsq_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_crtsq_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_CRT_SQ;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "q:s:p:k:";
    struct nvme_create_sq cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    { 
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.sqid = (u16)para;
                PRINT("crtsq.qid: %d.\n", cmd_para.sqid);
                break;

            case 's':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qsize = (u16)para;
                PRINT("crtsq.qsize: %d.\n", cmd_para.qsize);
                break;
            case 'p':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                para = para << 1;
                para = para + 1;                        /* physically contiguous fixed */
                cmd_para.sq_flags = (u16)para;          /* queue priority && physically contiguous */
                PRINT("crtsq.q_flags: %d.\n", cmd_para.sq_flags);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of crtsq: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-delcq", and excecute this function
* 
*FUNCTION NAME:parse_delcq_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_delcq_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_DEL_CQ;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "q:k:";
    struct nvme_delete_queue cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qid = (u16)para;
                PRINT("delcq.qid: %d.\n", cmd_para.qid);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of delcq: %c.\n", opt);
                break;
        }
    }


    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-crtsq", and excecute this function
* 
*FUNCTION NAME:parse_crtcq_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_crtcq_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_CRT_CQ;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "q:s:k:v:w:c:";
    struct nvme_create_cq cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.cqid = (u16)para;              
                cmd_para.irq_vector = cmd_para.cqid;   //default  irq_vector=qid
                PRINT("crtcq.qid: %d.\n", cmd_para.cqid);
                break;
                
            case 'v':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.irq_vector = (u16)para;       //override irq_vector
                PRINT("crtcq.irq_vector: %d.\n", cmd_para.irq_vector);
                break; 
                
            case 's':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qsize = (u16)para;
                PRINT("crtcq.qsize: %d.\n", cmd_para.qsize);
                break;

            case 'c':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.rsvd1[0] = (u32)para;
                PRINT("cq_where: %d.\n", cmd_para.rsvd1[0]);
                break;
                
            case 'w':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.rsvd1[1] = (u32)para;
                PRINT("sq_where: %d.\n", cmd_para.rsvd1[1]);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of crtcq: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-idn", and excecute this function
* 
*FUNCTION NAME:parse_idn_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_idn_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_IDN;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "i:c:o:f:e:k:";
    struct nvme_identify cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring)))
    {
        switch (opt)
        {
            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid = (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;

            case 'c':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.cns = (u32)para;
                PRINT("cns(controller or nsid struct): %d.\n", cmd_para.cns);
                break;
                
            case 'o':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp1_offt = (u16)para;
                PRINT("prp1 offt: %d.\n", cmd_para.prp1_offt);
                break;
                
            case 'e': 
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp2_offt = (u16)para;
                PRINT("prp2 offt: %d.\n", cmd_para.prp2_offt);
                break;

            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of idn: %c.\n", opt);
                break;
        }
    }

    /* cmd excecuting to be continued... */
    cmd_para.prp1 = (s64)malloc(PAGE_SIZE);
    if (0 == cmd_para.prp1) {
        printf("malloc failed.\n");
        return ERROR;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

    /* output */
    if (OK != parse_write_file(filename, (void*)cmd_para.prp1, PAGE_SIZE, 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
free_mem:
    free((void *)cmd_para.prp1);
    return ret;
}


/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-getlp", and excecute this function
* 
*FUNCTION NAME:parse_getlp_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_getlp_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_GET_LP;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "l:n:o:i:e:f:k:";
    struct nvme_get_log_page cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'l':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.lid = (u16)para;
                PRINT("log id: %d.\n", cmd_para.lid);
                break;

            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.ndw = (u16)para;
                PRINT("number dword: %d.\n", cmd_para.ndw);
                break;
                
           case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid = (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;

           case 'o':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp1_offt = (u16)para;
                PRINT("prp1 offt: %d.\n", cmd_para.prp1_offt);
                break;
                
           case 'e':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp2_offt = (u16)para;
                PRINT("prp2 offt: %d.\n", cmd_para.prp2_offt);
                break;

            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of getlp: %c.\n", opt);
                break;
        }
    }

    /* cmd excecuting to be continued... */
    cmd_para.prp1 = (s64)malloc((cmd_para.ndw + 1) << 2);   //0-base
    if (cmd_para.prp1 == 0) {
        perror("malloc");  
        return ERROR;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

    /* output */
    if (OK != parse_write_file(filename, (void*)cmd_para.prp1, ((cmd_para.ndw + 1) << 2), 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
free_mem:
    free((void *)(cmd_para.prp1));
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-fwdown", and excecute this function
* 
*FUNCTION NAME:parse_fwdown_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_fwdown_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    u32 length;
    int cmd = NEXUS_IOCTL_FWDOWN;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "n:o:r:k:f";
    struct nvme_download_firmware cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.ndw = (u32)para;
                PRINT("number dword: 0x%x.\n", cmd_para.ndw);
                break;

            case 'r':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.offset = (u32)para;
                PRINT("offset: 0x%x.\n", cmd_para.offset);
                break;
                
             case 'o':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp1_offt = (u32)para;
                PRINT("prp1 offset: 0x%x.\n", cmd_para.prp1_offt);
                break;
                
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of firmwaredownload: %c.\n", opt);
                break;
        }
    }

    cmd_para.prp1 = (s64)parse_read_file(filename, &length);
    if (cmd_para.prp1 == 0) {
        printf("read file failed.\n");
        return ERROR;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_ptr;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
free_ptr:
    free((void *)cmd_para.prp1);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-fwact", and excecute this function
* 
*FUNCTION NAME:parse_fwactv_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_fwactv_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_FWACTV;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    u8 aa = 0;
    u8 fs = 0;
    char* optstring = "a:k:s";
    struct nvme_activate_firmware cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    { 
        switch (opt) 
        {
            case 's':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                fs = (u8)para;
                PRINT("Firmware Slot: 0x%x.\n", fs);
                break;
                
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                aa = (u8)(para << 3 & 0x18);
                PRINT("Active Action: 0x%x.\n", (u8)para);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;

            default :
                printf("unexpected parameter of Firmware Active: %c.\n", opt);
                break;
        }
    }

    cmd_para.aafs = (u16)(aa | fs);
    PRINT("aafs: 0x%x.\n", cmd_para.aafs);

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-rstns", and excecute this function
* 
*FUNCTION NAME:parse_rstns_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_rstns_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_RST_NS;
    long para = 0;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "n:";
    u16 instance;
    u32 cmd_para;
    char name[DEV_NAME_SIZE];

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para = (u32)para;
                PRINT("nsid: %d.\n", cmd_para);
                break;

            default :
                printf("unexpected parameter of rstns: %c.\n", opt);
                break;
        }
    }

    instance = 0;
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);


    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-abort", and excecute this function
* 
*FUNCTION NAME:parse_abort_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_abort_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_ABORT;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "q:c:k:";
    struct nvme_abort_cmd cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.sqid = (u16)para;
                PRINT("g_cmd_line.abort.qid: %d.\n", cmd_para.sqid);
                break;
                
            case 'c':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.cid = (u16)para;
                PRINT("g_cmd_line.abort.cid: %d.\n", cmd_para.cid);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of abort: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-setft", and excecute this function
* 
*FUNCTION NAME:parse_setft_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_setft_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_SET_FT;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    u32 para_len;
    char* optstring = "i:u:f:s:d:o:e:k:";
    struct nvme_features cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'u':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.fid= (u32)para;
                PRINT("g_cmd_line.ft.fid: %d.\n", cmd_para.fid);
                break;

            case 's':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.sv = (u16)para;
                PRINT("g_cmd_line.ft.sv: %d.\n", cmd_para.sv);
                break;

            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid = (u32)para;
                PRINT("g_cmd_line.ft.nsid: %d.\n", cmd_para.nsid);
                break;
                
            case 'd':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.dword11 = (u32)para;
                PRINT("g_cmd_line.ft.dw11: 0x%x.\n", cmd_para.dword11);
                break;

            case 'o':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp1_offt = (u16)para;
                PRINT("setft prp1 offset: 0x%x.\n", cmd_para.prp1_offt);
                break;
                
            case 'e':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp2_offt = (u16)para;
                PRINT("setft prp2 offset: 0x%x.\n", cmd_para.prp2_offt);
                break;
                
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                    
            default :
                printf("unexpected parameter of setft: %c.\n", opt);
                break;
        }
    }

    switch (cmd_para.fid) {
        case 0x03:
            cmd_para.filelen = PAGE_SIZE;
            break;
        
        case 0x0c:
            cmd_para.filelen = 256;
            break;

        case 0x81:
            cmd_para.filelen = 16;
            break;

        default:
            cmd_para.filelen = 0;
            break;
    }

    if(0 != strlen(filename)) {         /* pass file */
        cmd_para.prp1 = (s64)parse_read_file(filename, &para_len);
    } else {
        PRINT("no file(prp unused)\n");
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_ptr;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
free_ptr:
    if (0 != strlen(filename)) {
        free((void *)cmd_para.prp1);
    }
    return ret;
}

/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-getft", and excecute this function
* 
*FUNCTION NAME:parse_getft_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_getft_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_GET_FT;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "i:u:s:f:o:e:k:d:";
    struct nvme_features cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'u':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.fid = (u8)para;
                PRINT("g_cmd_line.ft.fid: %d.\n", cmd_para.fid);
                break;
                
            case 's':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.sel = (u8)para;
                PRINT("g_cmd_line.ft.sel: %d.\n", cmd_para.sel);
                break;
     
            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid = (u32)para;
                PRINT("g_cmd_line.ft.nsid: %d.\n", cmd_para.nsid);
                break;
           
            case 'o':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp1_offt = (u16)para;
                PRINT("getft prp1 offset: 0x%x.\n", cmd_para.prp1_offt);
                break;
                
            case 'e':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.prp2_offt = (u16)para;
                PRINT("getft prp2 offset: 0x%x.\n", cmd_para.prp2_offt);
                break;
                
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;

            case 'd':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.dword11 = (u32)para;
                PRINT("cmd_para.dword11: 0x%x.\n", cmd_para.dword11);
                break;
                    
            default :
                printf("unexpected parameter of getft: %c.\n", opt);
                break;
        }
    }

    switch (cmd_para.fid) {
        case 0x03:
             cmd_para.filelen = PAGE_SIZE;
             break;
         
        case 0x0c:
             cmd_para.filelen = 256;
             break;
         
        case 0x81:
             cmd_para.filelen = 16;
             break;
    
        default:
             cmd_para.filelen = 0;
             break;
     }
    
    /* now only use prp1, and all get feature do not need a file, to be continued... */
    cmd_para.prp1 = (s64)malloc(PAGE_SIZE);
    if (0 == cmd_para.prp1) {
        printf("malloc failed.\n");
        return ERROR;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_ptr;
    }

    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

    /* output  */
    if (cmd_para.fid == 0x3 || cmd_para.fid == 0xc || cmd_para.fid == 0x81) {
        if (OK != parse_write_file(filename, (void *)cmd_para.prp1, cmd_para.filelen, 1)) {
            perror("write file");
            ret = ERROR;
            goto close_fd;                                            
        }
     } 

     else {
        goto close_fd;
     } 
    
close_fd:
    close(fd);
free_ptr:
    free((void *)cmd_para.prp1);
    return ret;
}


/*******************************************************************
*
*FUNCTION DESCRIPTION: parsing the cmd "-asyne", and excecute this function
* 
*FUNCTION NAME:parse_asyner_cmdline
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int parse_asyner_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_ASN_NER;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:";
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }

    if (0 > ioctl(fd, cmd, NULL)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

close_fd:
    close(fd);
    return ret;
}

int parse_wrregspa_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_WR_REG_SPACE;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:n:q:i:f:k:";
    struct nvme_rw_regspace cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u32 length;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    memset(&cmd_para, 0, sizeof(cmd_para));
    cmd_para.qid = 1;  //default

    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_addr= (u64)para;
                PRINT("start_addr: 0x%lx.\n", (u64)cmd_para.start_addr);
                break;
            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.ndw= (u32)para;
                PRINT("ndw to write: %d.\n", cmd_para.ndw);
                break;
            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;
            case 'q':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qid = (u32)para;     /*note qid which IO queue to run PPA command*/
                PRINT("qid: %d.\n", cmd_para.qid);
                break;
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of wrregspa: %c.\n", opt);
                break;
        }
    }
    cmd_para.prp1 = (s64)parse_read_file(filename, &length);
    if ((s64)ERROR == cmd_para.prp1) {
        printf("read file failed.\n");
        return ERROR;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_ptr;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
close_fd:
    close(fd);
free_ptr:
    free((void*)cmd_para.prp1);
    return ret;
}

int parse_rdregspa_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_RD_REG_SPACE;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:n:q:i:f:k:";
    struct nvme_rw_regspace cmd_para;
    char filename[MAX_FILE_NAME] = {0};
    u16 instance = 0;
    char name[DEV_NAME_SIZE];

    memset(&cmd_para, 0, sizeof(cmd_para));
    cmd_para.qid = 1;  //default
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.start_addr= (u64)para;
                PRINT("start_addr: 0x%llx.\n", cmd_para.start_addr);
                break;
            case 'n':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.ndw = (u32)para;
                PRINT("cmd_para.ndw: 0x%x.\n", cmd_para.ndw);
                break;
            case 'q':  
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.qid = (u32)para;
                PRINT("qid: %d.\n", cmd_para.qid);
                break;
            case 'i':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.nsid= (u32)para;
                PRINT("nsid: %d.\n", cmd_para.nsid);
                break;
            case 'f':
                para_num++;
                strncpy(filename, optarg, sizeof(filename)-1);
                PRINT("filename: %s.\n", filename);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of rdmem: %c.\n", opt);
                break;
        }
    }
    cmd_para.prp1 = (s64)malloc(cmd_para.ndw << 2);
    if (NULL == (void*)cmd_para.prp1) {
        perror("malloc");
        return ERROR;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    if (OK != parse_write_file(filename, (void *)cmd_para.prp1, cmd_para.ndw << 2, 1)) {
        perror("write file");
        ret = ERROR;
        goto close_fd;
    }
close_fd:
    close(fd);
free_mem:
    free((void*)cmd_para.prp1);
    return ret;
}

int parse_rddword_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_RD_DWORD;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:k:";
    struct rdmem_stru cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.mem_addr = (u32)para;
                PRINT("g_cmd_line.rdmem.mem_addr: 0x%x.\n", cmd_para.mem_addr);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of rddword: %c.\n", opt);
                break;
        }
    }
    cmd_para.pdata = malloc(sizeof(u32));  //Dword size
    if (NULL == cmd_para.pdata) {
        perror("malloc");
        return ERROR;
    }
    cmd_para.length   = 4;                  //dword:4byte

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free_mem;
    }
    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, 1);

    printf("0x%x\n", *(cmd_para.pdata));

    close(fd);
free_mem:
    free(cmd_para.pdata);
    return ret;
}

int parse_wrdword_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_WR_DWORD;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "a:v:k:";
    struct wrmem_stru cmd_para;
    u32 data;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'a':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                cmd_para.mem_addr = (u32)para;
                PRINT("g_cmd_line.wrmem.mem_addr: 0x%x.\n", cmd_para.mem_addr);
                break;
            case 'v':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                data = (u32)para;
                cmd_para.pdata = &data;
                PRINT("g_cmd_line.wrmem.pdata: 0x%x.\n", *cmd_para.pdata);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of wrreg: %c.\n", opt);
                break;
        }
    }

    cmd_para.length   = sizeof(u32);

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    ret = ioctl(fd, cmd, &cmd_para);
    parse_cmd_returnval(cmd, ret, instance, 1);
    
    close(fd);
    return ret;
}

int parse_datacheck_cmdline(int argc, char* argv[])
{
    int i;
    u32 data;
    u32 addr;
    u32 check;
    int ret = 0;
    int fd;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:";
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus");
        return ERROR;
    }
    
    srand( ( int ) time ( NULL) );
    addr = rand ()& DDR_ADDR_BUS;
    while ( addr > DDR_ADDR_DWORD )
    {
        addr = addr - DDR_ADDR_DWORD;
    }
    printf ( " addr = 0x%x\n", addr );
    data = 0x0;
    wr1dword(fd, addr, data);
    check = rd1dword(fd, addr);
    printf ( " check = 0x%x\n", check );
    data = DDR_DATA_BUS;
    wr1dword(fd, addr, data);
    check = rd1dword(fd, addr);
    printf (" check = 0x%x\n", check );
    data = 0x1;
    for ( i = 0; i < DDR_DATA_BUS_NUM; i++ )
    {
        wr1dword(fd, addr, data);
        check = rd1dword(fd, addr);
        if ( check != data ) {
            printf ( " ERROR i = %d\n" , i );
            printf ( " check = 0x%x, data = 0x%x\n" , check, data );
        }
        printf ( " check = %x\n" , check );
        data = data << 1;
    }
    for ( i = 0;i < DDR_DATA_BUS_NUM; i++ )
    {
        data = tran_data ( i );
        wr1dword(fd, addr, data);
        check = rd1dword (fd, addr);
        if ( check != data ) {
            printf ( " ERRORi = %d\n" , i );
            printf ( " check = 0x%x, data = 0x%x\n" , check , data );
        }
        printf ( " check = 0x%x\n" , check );
    }
    close(fd);
    return ret;
}

int parse_addrcheck_cmdline(int argc, char* argv[])
{
    u32 addr;
    u32 data;
    u32 check;
    int i;
    int ret = 0;
    int fd;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:";
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus");
        return ERROR;
    }
    
    printf("-------------------------move 1----------------------------\n");
    wr1dword(fd, 0x0,0x5a5a5a5a);
    addr = 0x4;                                      //dword, 0 and 1 bit is 0
    for ( i = 0; i < DDR_ADDR_BUS_NUM-2; i++ )
    {
        data = addr;
        wr1dword(fd, addr, data);
        addr = addr << 1;
    }
    check = rd1dword(fd, 0x0);
    data = 0x5a5a5a5a;
    if ( check != data ) {
        printf ( " error \n" );
        printf ( " addr = 0x%x \n" , addr );
        printf ( " check = 0x%x , data = 0x%x \n", check , data );
    }
    printf ( " check = %x \n", check);
    addr = 0x4;                                      //dword, 0 and 1 bit is 0
    for (  i = 0; i < DDR_ADDR_BUS_NUM-2; i++ )
    {
        data = addr;
        check = rd1dword(fd, addr);
        if ( check != data ) {
            printf ( " error " );
            printf ( " addr = 0x%x \n" , addr );
            printf ( " check = 0x%x , data = 0x%x \n", check , data );
        }
        printf ( " check = 0x%x \n", check);
        addr = addr << 1;
    }
    printf("-------------------------move 0----------------------------\n");
    wr1dword(fd, DDR_ADDR_DWORD, DDR_ADDR_DWORD);
    addr = DDR_ADDR_DWORD;
    for ( i = 2; i < DDR_ADDR_BUS_NUM; i++ )       //dword, 0 and 1 bit is 0
    {
        addr = tran_addr( i );
        data = addr;
        wr1dword(fd, addr, data);
    }
    check = rd1dword(fd, DDR_ADDR_DWORD);
    data = DDR_ADDR_DWORD;
    if ( check != data ) {
    printf ( " error \n" );
        printf ( " addr = 0x%x \n" , addr );
        printf ( " check = 0x%x , data = 0x%x \n", check , data );
    }
    printf ( " check = 0x%x \n", check);
    addr = DDR_ADDR_DWORD;
    for ( i = 2; i < DDR_ADDR_BUS_NUM; i++ )       //dword, 0 and 1 bit is 0
    {
        addr = tran_addr( i );
        data = addr;
        check = rd1dword(fd, addr);
        if ( check != data ) {
            printf ( " error " );
            printf ( "addr = 0x%x \n" , addr );
            printf ( " check = 0x%x , data = 0x%x \n", check , data );
        }
        printf ( " check = 0x%x \n", check);
    }
    close(fd);
    
    return ret;
}
int parse_memcheck_cmdline(int argc, char* argv[])
{
    u32 addr = 0x00;
    u32 data;
    u32 check;
    u32 num[PATTERN_NUM]={0x55555555,0x5a5a5a5a,0xa5a5a5a5,0x6b6b6b6b,0xb6b6b6b6,0xffffffff,0x11111111,0x00000000};
    int i;
    int j;
    u32 b[BURST_DWORD_NUM] = {0};
    int ret = 0;
    int fd;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:";
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of dumpnvme: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus");
        return ERROR;
    }
    
    for ( i = 0; i < PATTERN_NUM; i++ )
    {   
        addr = 0x0;
        printf ("write ddr\n");
        while ( addr < DDR_ADDR_BUS )
        {
            data = num[ i ];
            bwrdword(fd, addr, data);
            addr = addr + BURST_DWORD_NUM * 4;      // 128 dword is 128 * 4 bytes
            j++;
        }
        addr = 0x0;        
        printf ("read and check ddr\n");
        while(addr < DDR_ADDR_BUS)
        {           
            brddword(fd, addr, b);
            data = num[ i ];
            for (j = 0; j < BURST_DWORD_NUM; j++)
            {
                check = b[j];
                if ( check != data ){
                    printf ( " errori = %d\n " , i );
                    printf ( " check = 0x%x , data = 0x%x\n " , check , data );
                    close(fd);
                    return ERROR;
                }
            }
            addr = addr + BURST_DWORD_NUM * 4;    // 128 dword is 128 * 4 bytes
        }
        printf  ( "i = %d\n " , i );
    }
    close(fd);
    return ret;
}

int parse_autotest_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long para = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_AUTO_TEST;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "v:k:";
    struct wrmem_stru cmd_para;
    u32 data;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'v':
                para_num++;
                para = strtol(optarg, &endptr, 0);
                data = (u32)para;
                cmd_para.pdata = &data;
                PRINT("g_cmd_line.wrmem.pdata: 0x%x.\n", *cmd_para.pdata);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of autotest: %c.\n", opt);
                break;
        }
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
close_fd:
    close(fd);
    return ret;
}

int parse_badblock_cmdline(int argc, char *argv[])
{
    int flag = GOOD_PPA;
    int fd;    
    FILE *fp;
    long para_num = 0;
    int opt = 0;
    long para = 0;
    char* endptr;
    char* optstring = "m:f:t:k:";    
    char bad_address_file[MAX_FILE_NAME] = {0};
    char blk_info_file[MAX_FILE_NAME] = {0};
    int ret = 0; 
    u8 *pdata;
    u16 *blk_info_table;
    int cmd = NEXUS_IOCTL_PPA_SYNC;
    int blk_val, lun_val, ch_val, pl_val, pg_val, ep_val;    
    int offset;
    struct physical_address add_pg0_ep0;    
    struct physical_address add_pg0_ep3;   
    struct physical_address add_pg255_ep0;   
    struct physical_address add_pg255_ep3;
    struct physical_address add_pg257_ep3;
    
    u8 pg0_ep0_first_byte;    
    u8 pg0_ep3_first_byte;
    u8 pg255_ep0_first_byte;
    u8 pg255_ep3_first_byte;
    u8 pg257_ep3_first_byte;
    u32 bad_blk_cnt = 0;
    u32 flash_type = 0;
    struct nvme_ppa_command cmd_para;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    u32 channel;
    u16 chmask_hw;
    u32 metaraw_size = 304;
    
    memset(&cmd_para,0,sizeof(cmd_para));
    cmd_para.opcode = nvme_cmd_rdpparaw;
    cmd_para.nlb = 0;
    cmd_para.nsid = 1;
    cmd_para.apptag = 1;
    
    cmd_para.prp1 = (s64)malloc(PAGE_SIZE);
    if (cmd_para.prp1 == 0) {
        return -1;
    }
    cmd_para.metadata = (s64)malloc(PAGE_SIZE);
    if (cmd_para.metadata == 0) {
        ret = -1;
        goto free_prp1;
    }
    
    pdata = (u8 *)cmd_para.prp1;
    blk_info_table = malloc(CFG_NAND_BLOCK_NUM * /*sizeof(u16)*/(CH_NUM >> 3) * LUN_NUM );
    if (blk_info_table == NULL) {
        ret = -1;
        goto free_meta;
    }
    memset(blk_info_table, 0, CFG_NAND_BLOCK_NUM * /*sizeof(u16)*/(CH_NUM >> 3) * LUN_NUM);
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 't':           /*TOSHIBA OR MICRON OR SKHYNIX*/
                para_num++;
                para = strtol(optarg, &endptr, 0);
                flash_type = (u32)para;
                PRINT("flash_type: 0x%x.\n", flash_type);
                break;
            case 'f':
                para_num++;
                strncpy(bad_address_file, optarg, sizeof(bad_address_file)-1);
                PRINT("bad_address_file: %s.\n", bad_address_file);
                break;
            case 'm':
                para_num++;
                strncpy(blk_info_file, optarg, sizeof(blk_info_file)-1);
                PRINT("blk_info_file: %s.\n", blk_info_file);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of scan_badblock: %c.\n", opt);
                break;
        }
    }

    read_nvme_reg32(instance, MTDSZ, &metaraw_size);
    offset = PAGE_SIZE - 3*metaraw_size;

    read_nvme_reg32(instance, CHANNEL_COUNT, &channel);
    if(channel == 4) {
        chmask_hw = 0x1111;
    } else if(channel == 8) {
        chmask_hw = 0x3333;     
    } else {
        chmask_hw = 0xffff;
    }

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);           // open device
    if (0 > fd) {
        perror("open nexus0");
        ret = ERROR;
        goto free;
    }
    fp = fopen(bad_address_file , "a+");
    if(fp == NULL) {
        ret = ERROR;
        goto close_fd;
    }
    for(blk_val = CFG_NAND_BLOCK_NUM-1; blk_val >= 0; blk_val--)         // blk reverse scanf
    {
        for(lun_val = 0; lun_val < LUN_NUM; lun_val++)
        {            
            for(ch_val = 0; ch_val < CH_NUM; ch_val++)
            {
                flag = skip_maskchannel(ch_val, chmask_hw);
                if (flag == BAD_PPA) {
                    continue;
                }

                for(pl_val = 0; pl_val < PL_NUM; pl_val++)
                {
                    if(flash_type == 0)     /* TOSHIBA FLASH */
                    {
                        pg_val = 0;
                        ep_val = 0;
                        set_ppa_addr(&add_pg0_ep0, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg0_ep0.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                             perror("ioctl nexus0");
                             ret = ERROR;
                             goto close;
                        }
                        pg0_ep0_first_byte = *pdata;
                        //PRINT("pg0_ep0_first_byte=0x%x\n", pg0_ep0_first_byte);
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg0_ep0_first_byte != 0xff) {
                            PRINT("find bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d ppa_addr=0x%x\n", blk_val, lun_val, ch_val, pl_val, add_pg0_ep0.ppa);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d ppa_addr=0x%x\n", blk_val, lun_val, ch_val, pl_val, add_pg0_ep0.ppa);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                            //return 0;
                        }
                        

                        pg_val = 0;
                        ep_val = 3;
                        set_ppa_addr(&add_pg0_ep3, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg0_ep3.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                             perror("ioctl nexus0");
                             ret = ERROR;
                             goto close;
                        }
                        pg0_ep3_first_byte = *((u8 *)(cmd_para.prp1 + offset));
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg0_ep3_first_byte != 0xff) {
                            PRINT("find bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d\n ppa_addr=0x%x", blk_val, lun_val, ch_val, pl_val, add_pg0_ep3.ppa);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                        }


                        pg_val = 255;
                        ep_val = 0;
                        set_ppa_addr(&add_pg255_ep0, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg255_ep0.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                               perror("ioctl nexus0");
                               ret = ERROR;
                               goto close;
                        }
                        pg255_ep0_first_byte = *pdata;
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg255_ep0_first_byte != 0xff) {
                            PRINT("find bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d ppa_addr=0x%x\n", blk_val, lun_val, ch_val, pl_val, add_pg255_ep0.ppa);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d ppa_addr=0x%x\n", blk_val, lun_val, ch_val, pl_val, add_pg255_ep0.ppa);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                        }

                        pg_val = 255;
                        ep_val = 3;
                        set_ppa_addr(&add_pg255_ep3, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg255_ep3.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                             perror("ioctl nexus0");
                             ret = ERROR;
                             goto close;
                        }
                        pg255_ep3_first_byte = *((u8 *)(cmd_para.prp1 + offset));
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg255_ep3_first_byte != 0xff) {
                            PRINT("find bad phyblk k BLK:%d LUN:%d CH:%d PLANE:%d ppa_addr=0x%x\n", blk_val, lun_val, ch_val, pl_val, add_pg255_ep3.ppa);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d ppa_addr=0x%x\n", blk_val, lun_val, ch_val, pl_val, add_pg255_ep3.ppa);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                        }
                        
                        if (pg0_ep0_first_byte == 0xff && pg0_ep3_first_byte == 0xff &&
                            pg255_ep0_first_byte == 0xff && pg255_ep3_first_byte == 0xff) {
                            PRINT("good phyblk plane%d of BLK:%d LUN:%d CH:%d\n", pl_val, blk_val, lun_val, ch_val);/* good plane   (plane0--plane3 all good block is good) */
                        } 
                    }
                    else if(flash_type == 1)        /* MICRON FLASH */
                    {
                        pg_val = 0;
                        ep_val = 3;                      
                        set_ppa_addr(&add_pg0_ep3, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg0_ep3.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                             perror("ioctl nexus0");
                             ret = ERROR;
                             goto close;
                        }
                        pg0_ep3_first_byte = *((u8 *)(cmd_para.prp1 + offset));                     
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg0_ep3_first_byte != 0xff)
                        {
                            PRINT("find bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                        } else {
                            PRINT("good phyblk plane%d of BLK:%d LUN:%d CH:%d\n", pl_val, blk_val, lun_val, ch_val);/* good plane   (plane0--plane3 all good block is good) */
                            continue;
                        }
                    }
                    else if(flash_type == 3)        /* SKHYNIX FLASH */
                    {
                        pg_val = SB_MIN;
                        ep_val = 3;
                        set_ppa_addr(&add_pg0_ep3, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg0_ep3.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                             perror("ioctl nexus0");
                             ret = ERROR;
                             goto close;
                        }
                        pg0_ep3_first_byte = *((u8 *)(cmd_para.prp1 + offset));
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg0_ep3_first_byte != 0xff) {
                            PRINT("find bad phyblk BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                        }
                      
                        pg_val = ((WL_NUM-1)<<SB_BITS)+(SB_NUM);
                        ep_val = 3;
                        set_ppa_addr(&add_pg257_ep3, ch_val, ep_val, pl_val, lun_val, 0, pg_val, blk_val);
                        cmd_para.start_list = add_pg257_ep3.ppa;
                        if (0 > ioctl(fd, cmd, &cmd_para)) {
                             perror("ioctl nexus0");
                             ret = ERROR;
                             goto close;
                        }
                        pg257_ep3_first_byte = *((u8 *)(cmd_para.prp1 + offset));
                        memset(pdata, 0, PAGE_SIZE);
                        if (pg0_ep3_first_byte != 0xff) {
                            PRINT("find bad phyblk BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);
                            bad_blk_cnt++;
                            fprintf(fp, "bad phyblk  BLK:%d LUN:%d CH:%d PLANE:%d\n", blk_val, lun_val, ch_val, pl_val);       
                            blk_info_table[(CFG_NAND_BLOCK_NUM-1-blk_val)*LUN_NUM + lun_val] |= 1 << ch_val;
                            continue;
                        }

                        if (pg0_ep3_first_byte == 0xff && pg257_ep3_first_byte == 0xff) {
                            PRINT("good phyblk plane%d of BLK:%d LUN:%d CH:%d\n", pl_val, blk_val, lun_val, ch_val);/* good plane   (plane0--plane3 all good block is good) */
                            continue;
                        } 
                    }
                    else
                    {
                        PRINT("flash type input error");
                        goto close;
                    }
                }
            }
        }
    }
    fprintf(fp, "bad block total count: %d", bad_blk_cnt);
    PRINT("cnt:%d\n",bad_blk_cnt);
    if (OK != parse_write_file(blk_info_file, (void *)blk_info_table, CFG_NAND_BLOCK_NUM * /*sizeof(u16)*/(CH_NUM >> 3) * LUN_NUM, 1)) {
        perror("write file");
        ret = ERROR;
        goto close;
    }
close:    
   fclose(fp);
close_fd:
    close(fd);   
free:
    free((void *)blk_info_table);
free_meta:
    free((void *)cmd_para.metadata);
free_prp1:
    free((void *)cmd_para.prp1);
    return ret;
}

int parse_get_mac_addr_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_RD_REG64;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:i:";
    struct rdreg64_stru cmd_para;
    u64 data;
    u32 addr;
    u16 instance = 0;
    u32 nsid = 0;
    char name[DEV_NAME_SIZE];
    
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'i':
                para_num++;
                nsid = (u32)strtol(optarg, &endptr, 0);
                break;

            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            
            default :
                printf("unexpected parameter of rdreg64: %c.\n", opt);
                break;
        }
    }
    cmd_para.pdata = &data;

    if(nsid == 0){     //host mac addr
        addr = HOST_MAC_ADDR;
    }else{             //namespace mac addr
        addr = NAMESPACE_MAC_BASE + nsid * 8;
    }

    cmd_para.mem_addr = addr;

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }

    if(nsid == 0){   //host mac addr
        printf("device id:0x%x\n", instance);
        printf("host mac addr:0x%lx\n", data);
    }else{           //namespace mac addr
        printf("device id:0x%x\n", instance);
        printf("nsid:0x%x\n", nsid);
        printf("namespace mac addr:0x%lx\n", data);
    }
    
close_fd:
    close(fd);
    return ret;
}

int parse_set_mac_addr_cmdline(int argc, char* argv[])
{
    int ret = 0;
    long long para1;
    int fd = 0;
    int cmd = NEXUS_IOCTL_WR_REG64;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:i:v:l:";
    struct wrreg64_stru cmd_para;
    u64 data = 0;
    u16 instance = 0;
    u32 nsid = 0;
    u16 location = 0;
    u32 addr = 0;
    char name[DEV_NAME_SIZE];

    while (ERROR != (opt = getopt(argc, argv, optstring)))
    {
        switch (opt) 
        {
            case 'l':
                para_num++;
                location = (u16)strtol(optarg, &endptr, 0);
                break;
                
            case 'v':
                para_num++; 
                para1 = strtoll(optarg, &endptr, 0);
                data = (u64)para1;
                break;
                
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
                
            case 'i':
                para_num++;
                nsid = (u32)strtol(optarg, &endptr, 0);
                break;
                
            default :
                printf("unexpected parameter of wrreg64: %c.\n", opt);
                break;
        }
    }

    if(nsid == 0){            //host mac addr
        addr = HOST_MAC_ADDR;
    }else{                    //namespace mac addr
        addr = NAMESPACE_MAC_BASE + nsid * 8;
        
        if(location == 1){    //remote
            data = ((u64)1 << LOCATION_BIT) | data;
        }else{                //local
            data = ((u64)3 << LOCATION_BIT) | data;
        }   
    }

    cmd_para.mem_addr = addr;
    cmd_para.pdata    = &data;

    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &cmd_para)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
close_fd:
    close(fd);
    return ret;
}

int parse_set_db_switch_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_SET_DB_SWITCH;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "c:k:";
    int db_switch;
    u16 instance = 0;
    char name[DEV_NAME_SIZE];
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'c':
                para_num++;
                db_switch = strtol(optarg, &endptr, 0);
                PRINT("switch: %d.\n", db_switch);
                break;
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of crtsq: %c.\n", opt);
                break;
        }
    }
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &db_switch)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
close_fd:
    close(fd);
    return ret;
}

int parse_get_db_switch_cmdline(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    int cmd = NEXUS_IOCTL_GET_DB_SWITCH;
    long para_num = 0;
    char* endptr;
    int opt = 0;
    char* optstring = "k:";
    u16 instance = 0;
    int db_switch;
    char name[DEV_NAME_SIZE];
    while (ERROR != (opt = getopt(argc, argv, optstring))) 
    {
        switch (opt) 
        {
            case 'k':
                para_num++;
                instance = (u16)strtol(optarg, &endptr, 0);
                break;
            default :
                printf("unexpected parameter of crtsq: %c.\n", opt);
                break;
        }
    }
    snprintf(name, DEV_NAME_SIZE, "%s%d", NEXUS_DEV, instance);
    fd = open(name, O_RDWR);
    if (0 > fd) {
        perror("open nexus0");
        return ERROR;
    }
    if (0 > ioctl(fd, cmd, &db_switch)) {
        perror("ioctl nexus0");
        ret = ERROR;
        goto close_fd;
    }
    printf("nexus_dev[%d] db update switch:%d\n", instance, db_switch);
close_fd:
    close(fd);
    return ret;
}

