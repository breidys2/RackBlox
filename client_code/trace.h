#ifndef TRACE_H_
#define TRACE_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct StorageReq_ {
    int RW;
    uint32_t Addr;
    uint32_t vssd_id;
    char data[4096];
} StorageReq;


StorageReq* parse_trace(const char* fname, uint32_t n_events); 
#endif //TRACE_H_
