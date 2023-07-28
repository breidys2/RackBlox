#ifndef _RB_H
#define _RB_H
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
   
#define WINDOW_SZ 100
#define MAXLINE 5120
#define SPR8_PORT 8889
#define SPR8_IP "192.17.97.69"

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
} req_type;

typedef struct Message_ {
    req_type type;
    uint32_t vssd_id;
    uint64_t ing_lat;
    uint64_t eg_lat;
    uint64_t gen_ns;
    uint32_t addr;
    uint64_t lat;
} __attribute__((__packed__)) Message;

typedef struct MessageReply_ {
    Message msg;
    char data[4096];
} MessageReply;

typedef struct rb_node {
    MessageReply* msg;
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

rb_node_t* dev_q_head;
rb_node_t* dev_q_tail;
rb_node_t* reply_q_head;
rb_node_t* reply_q_tail;
rb_node_t* gc_q_head;
rb_node_t* gc_q_tail;
rb_node_t* gcresp_q_head;
rb_node_t* gcresp_q_tail;
double sliding_window;
double window_vals[WINDOW_SZ];
int win_ptr;

void free_node(rb_node_t* free_node) {
    free(free_node->msg);
    free(free_node);
}
void insert_list_sorted(rb_node_t* new_node, rb_node_t* head, rb_node_t* tail);
void insert_list_head(rb_node_t* new_node, rb_node_t* head);
rb_node_t* remove_list_tail(rb_node_t* tail);

#endif
