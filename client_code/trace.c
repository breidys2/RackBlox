#include "trace.h"

StorageReq* parse_trace(const char* fname, uint32_t n_events) {
    //Loop vars
    uint32_t i,j;
    //Filler except off, ret
    int ret;
    //File pointer
    FILE *fp;

    StorageReq* ret_reqs = (StorageReq*)malloc(n_events * sizeof(StorageReq));
    //Parse file for designated number of requests
    fp = fopen(fname, "r");
    for (i = 0; i < n_events; i++) {
        ret = fscanf(fp, "%d %d %d\n", &ret_reqs[i].RW, &ret_reqs[i].Addr, &ret_reqs[i].vssd_id);
        if (ret <= 0) {
            printf("Requested more events than trace had\n");
            exit(-1);
        }
        for (j = 0; j < 4096; j++) {
            ret_reqs[i].data[j] = rand()%256;
        }
    }
    fclose(fp);
    return ret_reqs;
}

