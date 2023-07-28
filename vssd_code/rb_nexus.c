#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "nexus.h"
#include "usage.h"
#include "blklist.h"
#include "rb_nexus.h"

//stdount, #include "channel.h"

//Block size is 16 KB page size * 256 pages = 4096 KB
#define BLK_NUM 60
#define FUN_SUCCESS 0
//First possible start channel is 0, last possible is 15
#define START_CHL 0
#define N_CHL 1
#define M_CHL 16
#define CHL_ID 5
#define N_TH 2
#define MAX_SECTOR (1 << 30)
inline double TimeGap( struct timeval* t_start, struct timeval* t_end ) {
    return (((double)(t_end->tv_sec - t_start->tv_sec) * 1000.0) +
            ((double)(t_end->tv_usec - t_start->tv_usec) / 1000.0));
}

typedef struct _thread_data_t {
    int n_blks;
    int index;
    u32 blks[BLK_NUM];
    u32 s_blks[BLK_NUM];
} thread_data_t;
static pthread_mutex_t llk;
static pthread_mutex_t dev_lock;
static pthread_mutex_t reply_lock;
static pthread_mutex_t gc_lock;
static pthread_mutex_t gc_resp_lock;
static const int BLK_SZ = PAGE_SIZE*4*256;
static const int PG_PER_BLK = 256;
static const int BLK_SZ_META = META_SIZE*4*256;
static int cnt =0;
static int target = 1;

char *buf;
char *metabuf;
char *readbuf;
char *readmetabuf;
void insert_list_head(rb_node_t* new_node, rb_node_t* head) {
    rb_node_t* pnode = head;
    rb_node_t* nnode = head->next;
    pnode->next = new_node;
    new_node->next = nnode;
    new_node->prev = pnode;
    nnode->prev = new_node;
}

//Significant reorderings are not the common case
void insert_list_sorted(rb_node_t* new_node, rb_node_t* head, rb_node_t* tail) {
    rb_node_t* cur_node = head;
    while(cur_node->next != tail && new_node->prio > cur_node->next->prio) {
        cur_node = cur_node->next;
    }
    //Do the insertion
    rb_node_t* pnode = cur_node;
    rb_node_t* nnode = cur_node->next;
    pnode->next = new_node;
    new_node->next = nnode;
    new_node->prev = pnode;
    nnode->prev = new_node;
}

rb_node_t* remove_list_tail(rb_node_t* tail) {
    rb_node_t* nnode = tail;
    rb_node_t* rnode = tail->prev;
    rb_node_t* pnode = tail->prev->prev;
    if (pnode == NULL) {
        perror("bad list removal");
        exit(-1);
    }
    rnode->prev = NULL;
    rnode->next = NULL;
    nnode->prev = pnode;
    pnode->next = nnode;
    return rnode;
}

void * run_net(void *args) {
       
    char buffer[MAXLINE];
    socklen_t len;
    int n;
    int recv_socket;
    struct sockaddr_in servaddr, cliaddr;
    memset(&window_vals, 0, WINDOW_SZ*sizeof(double));
    win_ptr = 0;
    sliding_window = 0;
       
    // Creating socket file descriptor
    if ( (recv_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
       
    int port = *(int*)args;
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
       
    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
       
    // Bind the socket with the server address
    if ( bind(recv_socket, (const struct sockaddr *)&servaddr, 
            sizeof(servaddr)) < 0 ) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    //Set read timeout for later
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 10;
    setsockopt(recv_socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
   
    len = sizeof(cliaddr);  //len is value/result
   
    printf("waiting\n");
    while(1) {
        n = recvfrom(recv_socket, (char *)buffer, MAXLINE, 
                    MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                    &len);
        if (n > 0) {
            //TODO have the writes be duplicated to the other vssd
            MessageReply* msg = (MessageReply*)malloc(sizeof(MessageReply));
            memcpy(msg, buffer, n);
            printf("Recv pkt with type: %d\n", msg->msg.type);
            rb_node_t* new_node = (rb_node_t*) malloc(sizeof(rb_node_t));
            new_node->msg = msg;

            //Update sliding window
            sliding_window -= window_vals[win_ptr]/WINDOW_SZ;
            window_vals[win_ptr] = msg->lat;
            sliding_window += msg->lat/WINDOW_SZ;
            win_ptr = (win_ptr + 1) % WINDOW_SZ;

            //Set the combined prio 
            new_node->prio = (uint64_t) (sliding_window + msg->lat - get_cur_ns());

            //Insert
            pthread_mutex_lock(&dev_lock);
            insert_list_sorted(new_node, dev_q_head, dev_q_tail);
            pthread_mutex_unlock(&dev_lock);
        }
        //Need to recv something first
        if (cliaddr.sin_port == 0) continue;
        //Check for data to send back
        rb_node_t* reply_node = NULL;
        pthread_mutex_lock(&reply_lock);
        if (reply_q_head->next!=reply_q_tail) {
            reply_node = remove_list_tail(reply_q_tail);
        }
        pthread_mutex_unlock(&reply_lock);
        if (reply_node != NULL) {
            MessageReply* msg_reply = reply_node->msg;
            //send data back
            memcpy(buffer, msg_reply, sizeof(MessageReply));
            n = sendto(recv_socket, buffer, sizeof(MessageReply), 0, (struct sockaddr *) &cliaddr, len);
            free_node(reply_node);
        }
        //Check GC q
        reply_node = NULL;
        pthread_mutex_lock(&gc_lock);
        if (gc_q_head->next!=gc_q_tail) {
            reply_node = remove_list_tail(gc_q_tail);
        }
        pthread_mutex_unlock(&gc_lock);
        if (reply_node != NULL) {
            MessageReply* msg_reply = reply_node->msg;
            //Send GC Packet to switch
            memcpy(buffer, msg_reply, sizeof(MessageReply));
            n = sendto(recv_socket, buffer, sizeof(MessageReply), 0, (struct sockaddr *) &cliaddr, len);
            free_node(reply_node);
        }

    }
    pthread_exit(NULL);
}

void * run_worker(void * args) {
    srand(time(NULL));
    thread_data_t * data = (thread_data_t *) args;
	struct timeval start, end;
    
    while(1) {
        rb_node_t* reply_node = NULL;
        pthread_mutex_lock(&dev_lock);
        if (dev_q_head->next != dev_q_tail) {
            reply_node = remove_list_tail(dev_q_tail);
        }
        pthread_mutex_unlock(&dev_lock);
        //execute received request
        if (reply_node != NULL) {
            printf("Fetched req with type: %d\n", reply_node->msg->msg.type);
            if (reply_node->msg->msg.type == rb_read) {
                //TODO execute read
                reply_node->msg->msg.type == rb_read_reply;
            } else {
                //TODO execute write
                reply_node->msg->msg.type == rb_write_reply;
            }
            pthread_mutex_lock(&reply_lock);
            insert_list_head(reply_node, reply_q_head);
            pthread_mutex_unlock(&reply_lock);
        }
    }

    pthread_exit(NULL);
}

void * run_gc(void * args) {
    //Parse input args
    while(1) {
        //Check GC status
        //If hit soft -> send gc req
        //Receive GC -> accept -> do gc
        //Receive gc deny -> do nothing
        //if hit hard -> send gc packet
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

    int rc;
    int port = atoi(argv[1]);
    pthread_mutex_init(&dev_lock, NULL);
    pthread_mutex_init(&reply_lock, NULL);
    pthread_mutex_init(&gc_lock, NULL);
    pthread_mutex_init(&gcresp_lock, NULL);

    //Init list comm lists
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

    pthread_t net_thread;
    if ((rc = pthread_create(&net_thread, NULL, run_net, &port))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }
    pthread_t gc_thread;
    //TODO update thread data
    if ((rc = pthread_create(&gc_thread, NULL, run_gc, NULL))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }
    pthread_t worker_thread;
    //TODO update thread data
    if ((rc = pthread_create(&worker_thread, NULL, run_worker, NULL))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }


    pthread_join(worker_thread, NULL);
    pthread_join(net_thread, NULL);
    pthread_join(gc_thread, NULL);
    //TODO have the implement/request GC
    //TODO server reads/writes in threads that pull from the queue

    
    return 0;

    /*
    buf = (char*)malloc(BLK_SZ);
    metabuf = (char*)malloc(BLK_SZ_META);
    readbuf = (char*)malloc(BLK_SZ);
    readmetabuf = (char*)malloc(BLK_SZ_META);
    pthread_mutex_init(&llk, NULL);

    int i,j,ret,rc;
    for(i=0; i<BLK_SZ; i++)  buf[i] = 'x';
    for(i=0; i<BLK_SZ_META; i++) metabuf[i] = 'm';
    
    
    //Worst case is 32 since we could have harvesting for each channel, unlikely to actually hit tho
    thread_data_t gc_thread_data;
    pthread_t gc_thread;
    thread_data_t write_thread_data;
    pthread_t write_thread;

    int chl_id = CHL_ID;
 	ret = alloc_channel(chl_id);

    //Create GC thread
    for(i=0; i<BLK_NUM; i++) {
        gc_thread_data.blks[i] = alloc_block(chl_id);
        write_thread_data.blks[i] = alloc_block(chl_id);
        gc_thread_data.s_blks[i] = alloc_block(chl_id);
    }
    //if ((rc = pthread_create(&gc_thread, NULL, run_gc, &gc_thread_data))) {
    //    fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
    //    return EXIT_FAILURE;
    //}
    if ((rc = pthread_create(&write_thread, NULL, run_wr, &write_thread_data))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }
    //pthread_join(gc_thread, NULL);
    pthread_join(write_thread, NULL);
    
    //TODO other stuff between experiments / cleanup
    
    free(buf);
    free(metabuf);
    free(readbuf);
    free(readmetabuf);
    return 0;
    */
} 
