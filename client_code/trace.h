#ifndef TRACE_H_
#define TRACE_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct StorageReq_ {
    char RW;
    uint32_t Addr;
    char data[4096];
} StorageReq;


StorageReq* parse_trace(const char* fname, uint32_t n_events); 
#endif //TRACE_H_
