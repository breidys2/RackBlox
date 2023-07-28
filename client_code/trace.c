#include "trace.h"

StorageReq* parse_trace(const char* fname, uint32_t n_events) {
    //Loop vars
    uint32_t i,j;
    //Filler except off, ret
    int a,b,c,off,size,ret;
    //Filler except type
    char complete, plus, aa[20], bb[20], type[20];
    //Filler
    double ts;
    //File pointer
    FILE *fp;

    StorageReq* ret_reqs = (StorageReq*)malloc(n_events * sizeof(StorageReq));
    //Parse file for designated number of requests
    fp = fopen(fname, "r");
    for (i = 0; i < n_events; i++) {
        ret = fscanf(fp, "%s %d %d %lf %d %c %s %d %c %d %s",aa,&a,&b,&ts,&c,&complete,type,&off,&plus,&size,bb);
        if (ret <= 0) {
            printf("Requested more events than trace had\n");
            exit(-1);
        }
        ret_reqs[i].Addr = off/8;
        ret_reqs[i].RW = type[0];
        for (j = 0; j < 4096; j++) {
            ret_reqs[i].data[j] = rand()%256;
        }
    }
    fclose(fp);
    return ret_reqs;
}
