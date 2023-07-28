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
*2014.2.27, wxu, initial coding.
*2015.3.25, ychen, copied from unvme and updated
*
****************************************************************/
 
//#include "nvme.h"
#include "nexus.h"
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

//extern struct cmd_line_stru g_cmd_line;
/*******************************************************************
*
*FUNCTION DESCRIPTION: usage description, for help display
* 
*FUNCTION NAME:usage
*
*PARAMTERS:
*        INPUT:
*        OUTPUT: 
*        RETURN:
*
*******************************************************************/
int usage (int argc, char* argv[])
{
    printf("%s version: %s\n", argv[0], NEXUS_VER);

    /*  cmd list help to be continued...  */
    printf("%s -help                                                                     Print help of usage\n", argv[0]);
    printf("-----------------------------------------------------------------------------------------------------------------\n");
    printf("Diag command:\n");
    printf("%s -rdreg -r regaddr(the start 4MB is register remap zone remianing is DDR)  Read Register\n", argv[0]);
    printf("%s -wrreg -r regaddr -v value                                                Write Register\n", argv[0]);    
    printf("%s -rdreg64 -r regaddr                                                       Read Register(64bit)\n", argv[0]);
    printf("%s -wrreg64 -r regaddr -v value                                              Write Register(64bit)\n", argv[0]);
    printf("%s -dumppci                                                                  Dump PCI register\n", argv[0]);
    printf("%s -dumpnexus -k instance                                                    Dump Nexus register\n", argv[0]); 
    printf("%s -checkcq -q qid                                                           check  CQ\n", argv[0]);
    printf("%s -checksq -q qid                                                           check  SQ\n", argv[0]);    

    printf("%s -rddword -a memaddr(0x00~0x1ffffffc)                                      DDR IMM mode read\n", argv[0]);
    printf("%s -wrdword -a memaddr(0x00~0x1ffffffc) -v value                             DDR IMM mode write\n", argv[0]);
    printf("%s -autotest -v value                                                        DDR auto-fill auto-verify\n", argv[0]);
    printf("%s -datacheck                                                                DDR check data bus\n", argv[0]);
    printf("%s -addrcheck                                                                DDR check address bus\n", argv[0]);    
    printf("%s -memcheck                                                                 DDR Burst W/R data consistency check\n", argv[0]);
    
    printf("-----------------------------------------------------------------------------------------------------------------\n");
    printf("Admin command:\n");
    printf("%s -crtcq -q qid -s qsize                                                    Create I/O CQ\n", argv[0]);    
    printf("%s -crtsq -q qid -s qsize -p prior                                           Create I/O SQ\n", argv[0]);
    printf("%s -delsq -q qid                                                             Delete I/O SQ\n", argv[0]);
    printf("%s -delcq -q qid                                                             Delete I/O CQ\n", argv[0]);
    printf("%s -getlp -l lid -n ndw -i nsid -o prp1_offt -e prp2_offt                    Get Log Page\n", argv[0]);
    printf("%s -idn -i nsid -c cns -o prp1_offt -e prp2_offt -f file                     Identify\n", argv[0]);
    printf("%s -setft -i nsid -u fid -s sv -d dw11 -o prp1_offt -e prp2_offt -f file     Set Features\n", argv[0]);
    printf("%s -getft -i nsid -u fid -s sel -o prp1_offt -e prp2_offt -f file            Get Features\n", argv[0]);    
    printf("%s -fwdown -n ndw -r offt -o prp1_offt -f Imagefile                          Firmware Image Download\n", argv[0]);    
    printf("%s -fwactv -a aa -f fwslot                                                   Firmware Activate specifed in fs\n", argv[0]);  
    printf("%s -abort -q qid -c cid                                                      Abort\n", argv[0]);
    printf("%s -asyner                                                                   Asynchronous Event Request\n", argv[0]);
    printf("%s -fmtnvm -i nsid -v dw10                                                   Format NVM\n", argv[0]);    

    printf("----------------------------------------------------------------------------------------------------------------------------\n");
    printf("PPA sync command(default value qid=1 instance=0):\n");
    
    // the following two commands can accept "-x xor" but skipped for external release. use utest instead.
    printf("%s -rdppasync -k instance -a ppaaddr -n nlb -i nsid -q qid -c ch(0)ep(1) -t ctrl -f data -m meta  Sync PPA Read\n", argv[0]);
    printf("%s -wrppasync -k instance -a ppaaddr -n nlb -i nsid -q qid -c ch(0)ep(1) -t ctrl -f data -m meta  Sync PPA Write\n", argv[0]);
    printf("%s -rdpparawsync -k instance -a ppaaddr -n nlb -i nsid -q qid -c ch(0)ep(1) -f data -m meta -x dw13      Sync PPA Read Raw\n", argv[0]);
    printf("%s -wrpparawsync -k instance -a ppaaddr -n nlb -i nsid -q qid -c ch(0)ep(1) -f data -m meta              Sync PPA Write Raw\n", argv[0]);	
    printf("%s -ersppasync -k instance -a ppaaddr -i nsid -q qid -n nlb -c ch(0)pl(2)                      Sync PPA Erase\n", argv[0]);
    printf("%s -badblock -k instance -t flash_type(TOSHIBA 0  MICRON 1) -f addr -m badmark.bin             Bad Block Detect\n", argv[0]);
    printf("%s -ersblock -k instance -i nsid -q qid -b block -m chmask_sw -f badmark.bin                   Erase one raid block\n", argv[0]);    
    printf("%s -ersall -k instance -i nsid -q qid -m chmask_sw -f badmark.bin                              Erase Whole disk \n", argv[0]);

    printf("-----------------------------------------------------------------------------------------------------------------\n");    
    printf("Vendor Specify command:\n");
    printf("%s -rdregspa -a addr -n ndw -i nsid -q qid -f file                           Read Register Space\n", argv[0]);
    printf("%s -wrregspa -a addr -n ndw -i nsid -q qid -f file                           Write Register Space)\n", argv[0]);
    printf("%s -rstns -n namespace                                                       Reset Namespace\n", argv[0]);

    return OK;
}



