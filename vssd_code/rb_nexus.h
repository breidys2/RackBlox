#ifndef _RB_H
#define _RB_H
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
 #include "vssd.h"
   
#define WINDOW_SZ 100
#define MAXLINE 5120
#define SPR8_PORT 8889
#define SPR8_IP "192.17.97.69"



//Potential packet types
typedef enum {
    rb_write = 0,
    rb_write_reply, //1
    rb_read,        //2
    rb_read_reply,  //3
    rb_gc,          //4
    rb_gc_finish,   //5
    rb_gc_request,  //6
    rb_gc_success,  //7
    rb_gc_deny,     //8
    rb_gc_fin_suc,     //9
} req_type;

//RB Message 
typedef struct Message_ {
    req_type type;
    uint32_t vssd_id;
    uint64_t ing_lat;
    uint64_t eg_lat;
    uint64_t gen_ns;
    uint32_t addr;
    uint64_t lat;
    char data[4096];
} __attribute__((__packed__)) Message;


//DELL node 
typedef struct rb_node {
    Message* msg;
    int64_t prio;
    struct rb_node* next;
    struct rb_node* prev;
} rb_node_t;

// Get current time in ns
uint64_t get_cur_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
    return t;
}

uint64_t expire_time = 60l * 1000 * 1000 * 1000;
uint64_t check_exp(struct timespec* start) {
    uint64_t start_ts = start->tv_sec * 1000 * 1000 * 1000 + start->tv_nsec;
    return (get_cur_ns() - start_ts) > expire_time ? 1 : 0;
}

int vSSD_id;

//GC Stuff
double soft_thresh = 0.35;
double hard_thresh = 0.25;

//Communication queues
rb_node_t* dev_q_head;
rb_node_t* dev_q_tail;
rb_node_t* reply_q_head;
rb_node_t* reply_q_tail;
rb_node_t* gc_q_head;
rb_node_t* gc_q_tail;
rb_node_t* gcresp_q_head;
rb_node_t* gcresp_q_tail;

//Sliding window
double sliding_window;
double window_vals[WINDOW_SZ];
int win_ptr;

//vSSD stuff
int read_prep = 50;
int chl_num = 1;
int cur_read = 0;


//Communicatoin functions
void free_node(rb_node_t* free_node) {
    free(free_node->msg);
    free(free_node);
}
void insert_list_sorted(rb_node_t* new_node, rb_node_t* head, rb_node_t* tail);
void insert_list_head(rb_node_t* new_node, rb_node_t* head);
rb_node_t* remove_list_tail(rb_node_t* tail);

//Init function for comm lists
void init_comm_lists() {
    dev_q_head = (rb_node_t*)malloc(sizeof(rb_node_t));
    dev_q_tail = (rb_node_t*)malloc(sizeof(rb_node_t));
    reply_q_head = (rb_node_t*)malloc(sizeof(rb_node_t));
    reply_q_tail = (rb_node_t*)malloc(sizeof(rb_node_t));
    gc_q_head = (rb_node_t*)malloc(sizeof(rb_node_t));
    gc_q_tail = (rb_node_t*)malloc(sizeof(rb_node_t));
    gcresp_q_head = (rb_node_t*)malloc(sizeof(rb_node_t));
    gcresp_q_tail = (rb_node_t*)malloc(sizeof(rb_node_t));
    dev_q_head->next = dev_q_tail;
    dev_q_tail->prev = dev_q_head;
    dev_q_head->prio = LONG_MAX;
    dev_q_tail->prio = LONG_MIN;
    reply_q_head->next = reply_q_tail;
    reply_q_tail->prev = reply_q_head;
    gc_q_head->next = gc_q_tail;
    gc_q_tail->prev = gc_q_head;
    gcresp_q_head->next = gcresp_q_tail;
    gcresp_q_tail->prev = gcresp_q_head;

}

#endif
