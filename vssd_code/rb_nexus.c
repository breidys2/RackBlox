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
    vssd_t * vssd;
    uint32_t* mapping;
    uint32_t* rev_mapping;
    int index;
} thread_data_t;
static pthread_mutex_t dev_lock;
static pthread_mutex_t reply_lock;
static pthread_mutex_t gc_lock;
static pthread_mutex_t gcresp_lock;
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
	struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    uint64_t prev_ns = 0;
   
    printf("waiting\n");
    sleep(5);
    while(1) {
        if(check_exp(&start)) {
            break;
        }
        n = recvfrom(recv_socket, (char *)buffer, MAXLINE, 
                    MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                    &len);
        if (n > 0) {
            Message* msg = (Message*)malloc(sizeof(Message));
            memcpy(msg, buffer, n);
            printf("Recv pkt with type: %d\n", msg->type);
            rb_node_t* new_node = (rb_node_t*) malloc(sizeof(rb_node_t));
            new_node->msg = msg;

            if (msg->type == rb_read || msg->type == rb_write) {
                //Update sliding window
                sliding_window -= window_vals[win_ptr]/WINDOW_SZ;
                window_vals[win_ptr] = msg->lat;
                sliding_window += msg->lat/WINDOW_SZ;
                win_ptr = (win_ptr + 1) % WINDOW_SZ;
                msg->lat += sliding_window;

                //Set the combined prio 
                uint64_t cur_ns = get_cur_ns();
                new_node->prio = (uint64_t) (msg->lat - cur_ns);

                //Update sliding window for background GC
                if (prev_ns == 0) prev_ns = cur_ns;
                idle_time = (idle_time * alpha) + (1-alpha) * (cur_ns-prev_ns);
                prev_ns = cur_ns;

                //Insert
                pthread_mutex_lock(&dev_lock);
                insert_list_sorted(new_node, dev_q_head, dev_q_tail);
                pthread_mutex_unlock(&dev_lock);
            } else if (msg->type == rb_gc_deny || msg->type == rb_gc_success || msg->type == rb_gc_fin_suc) {
                //Insert
                pthread_mutex_lock(&gcresp_lock);
                printf("Inserting into gcresp\n");
                insert_list_sorted(new_node, gcresp_q_head, gcresp_q_tail);
                pthread_mutex_unlock(&gcresp_lock);
            }
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
            Message* msg_reply = reply_node->msg;
            //send data back
            printf("Send reply type: %d\n", reply_node->msg->type);
            memcpy(buffer, msg_reply, sizeof(Message));
            n = sendto(recv_socket, buffer, sizeof(Message), 0, (struct sockaddr *) &cliaddr, len);
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
            Message* msg_reply = reply_node->msg;
            //Send GC Packet to switch
            printf("Sent gc packet to spr8\n");
            memcpy(buffer, msg_reply, sizeof(Message));
            n = sendto(recv_socket, buffer, sizeof(Message), 0, (struct sockaddr *) &cliaddr, len);
            free_node(reply_node);
        }
    }
    pthread_exit(NULL);
}

void * run_worker(void * args) {
    srand(time(NULL));
    //Parse input args
    thread_data_t * thr_data = (thread_data_t *)args;
    uint32_t* mapping_table = thr_data->mapping;
    uint32_t* rev_mapping_table = thr_data->rev_mapping;
    int read_wrap = read_prep * 1024;
    vssd_t* v = thr_data->vssd;
    int cur_page = 0;
	struct timespec start;


    //Prep the read blocks
    int i,j,k;
    uint32_t * prepped_reads = calloc(read_prep * 1024, sizeof(uint32_t));
    for(k = 0; k < chl_num; k++) {
        int index = v->allocated_chl[k];
        for(i = 0; i < read_prep; i++) {
            uint32_t lba = alloc_block_v(v, index);
            for(j = 0; j < 1024; j++) {
                int cur_ind = i * 1024 + j;
                prepped_reads[cur_ind] = lba * 1024 + j;
            }
            write_block_v(v,lba, buf, metabuf);
        }
    }

    printf("vSSD: %d is ready\n", thr_data->index);
    uint32_t cur_lba = alloc_block_v(v, v->allocated_chl[0]);
    rb_node_t* reply_node;
    
    clock_gettime(CLOCK_REALTIME, &start);
    while(1) {

        if(check_exp(&start)) {
            break;
        }
        reply_node = NULL;
        pthread_mutex_lock(&dev_lock);
        if (dev_q_head->next != dev_q_tail) {
            reply_node = remove_list_tail(dev_q_tail);
        }
        pthread_mutex_unlock(&dev_lock);
        //execute received request
        if (reply_node != NULL) {
            printf("Fetched req with type: %d\n", reply_node->msg->type);
            if (reply_node->msg->type == rb_read) {
                uint32_t map_index = prepped_reads[cur_read];
                cur_read = (cur_read + 1) % read_wrap;
                //Get the block and page indices from our mapped value, need these for the read func
                uint32_t block_ind = map_index/1024;
                uint32_t page_ind = map_index%1024;
                read_page_v(v,block_ind, page_ind, readbuf, readmetabuf);
                memcpy(reply_node->msg->data, readbuf, 4096);
                reply_node->msg->type = rb_read_reply;
            } else {
                //Mark for internal GC
                if (mapping_table[reply_node->msg->addr] != 0) {
                    invalidate_page(v, mapping_table[reply_node->msg->addr]);
                    rev_mapping_table[mapping_table[reply_node->msg->addr]] = 0;
                }
                //Execute write
                mapping_table[reply_node->msg->addr] = cur_lba * 1024 + cur_page;
                rev_mapping_table[cur_lba*1024 + cur_page] = reply_node->msg->addr;
                cur_page++;
                if (cur_page == 1024) {
                    write_block_v(v,cur_lba, buf, metabuf);
                    cur_lba = alloc_block_v(v, v->allocated_chl[0]);
                    cur_page = 0;
                }
                reply_node->msg->type = rb_write_reply;
            }
            pthread_mutex_lock(&reply_lock);
            insert_list_head(reply_node, reply_q_head);
            printf("Inserting reply type: %d\n", reply_node->msg->type);
            pthread_mutex_unlock(&reply_lock);
        }
    }
    //Cleanup all blocks
    erase_blks_v(v, v->max_addr);
    pthread_exit(NULL);
}

void * run_gc(void * args) {

    //Parse input args
    thread_data_t * thr_data = (thread_data_t *)args;
    uint32_t* mapping_table = thr_data->mapping;
    uint32_t* rev_mapping_table = thr_data->rev_mapping;
    vssd_t* v = thr_data->vssd;

	struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);

    while(1) {
        if(check_exp(&start)) {
            break;
        }
        //Check GC status
        double cur_gc_thresh = get_gc_thresh(v);
        //double cur_gc_thresh = 0.45;
        //if hit hard -> send gc packet
        if (cur_gc_thresh < soft_thresh || (cur_gc_thresh < bg_thresh && idle_time > idle_thresh)) {
            printf("gc_thresh: %f\n", cur_gc_thresh);
            //Generate the Message
            rb_node_t* new_node = (rb_node_t*)malloc(sizeof(rb_node_t));
            new_node->msg = (Message*)malloc(sizeof(Message));
            new_node->msg->vssd_id = vSSD_id;
            if (cur_gc_thresh < hard_thresh || (cur_gc_thresh < bg_thresh && idle_time > idle_thresh)) {
                new_node->msg->type = rb_gc;
            } else {
                new_node->msg->type = rb_gc_request;
            }
            pthread_mutex_lock(&gc_lock);
            printf("Sending gc req with type %d for %d\n", new_node->msg->type, new_node->msg->vssd_id);
            insert_list_head(new_node,gc_q_head);
            pthread_mutex_unlock(&gc_lock);
        } 
        rb_node_t * reply_node = NULL;
        pthread_mutex_lock(&gcresp_lock);
        if (gcresp_q_head->next!=gcresp_q_tail) {
            reply_node = remove_list_tail(gcresp_q_tail);
        }
        pthread_mutex_unlock(&gcresp_lock);
        if (reply_node != NULL) {
            printf("recv gc resp packet\n");
            Message* msg_reply = reply_node->msg;
            if (msg_reply->type == rb_gc_success) {
                //Receive gc success -> do gc
                printf("Received gc success\n");
                doing_gc = 1;
                do_gc(v,mapping_table,rev_mapping_table);
                doing_gc = 0;
                reply_node->msg->type = rb_gc_finish;
                pthread_mutex_lock(&gc_lock);
                printf("Sending gc finish reply\n");
                insert_list_head(reply_node,gc_q_head);
                pthread_mutex_unlock(&gc_lock);
                continue;
                //Send gc finish
            } else if (msg_reply->type == rb_gc_deny || msg_reply->type == rb_gc_fin_suc) {
                //Receive gc deny or ack for gc finish -> do nothing
                printf("Received gc deny\n");
            } else {
                printf("Got bad gc response!\n");
            }
            free(reply_node);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {

    int rc, i;
    if (argc < 3) {
        printf("Please provide <port_num> <timeout>\n");
        exit(-1);
    }
    int port = atoi(argv[1]);
    expire_time = atoi(argv[2]);
    //To ns
    expire_time *= 1000L * 1000L * 1000L;
    
    pthread_mutex_init(&dev_lock, NULL);
    pthread_mutex_init(&reply_lock, NULL);
    pthread_mutex_init(&gc_lock, NULL);
    pthread_mutex_init(&gcresp_lock, NULL);

    vSSD_id = port - 8885;
    //Make sure vSSDs are on disjoint channels (hw-isolation)
    for(i = 0; i < vSSD_id-1; i++) {
        alloc_chl |= (1 << i);
    }

    buf = (char*)malloc(BLK_SZ);
    memset(buf, 'x', BLK_SZ);
    metabuf = (char*)malloc(BLK_SZ_META);
    memset(metabuf, 'z', BLK_SZ_META);
    readbuf = (char*)malloc(BLK_SZ);
    memset(readbuf, 'x', BLK_SZ);
    readmetabuf = (char*)malloc(BLK_SZ_META);
    memset(readmetabuf, 'z', BLK_SZ_META);

    //Init list comm lists
    init_comm_lists();

    //Setup vSSD
    vssd_t * vssd_1;
    int chl_vssd = 1;
    vssd_1 = alloc_regular_vssd(chl_vssd);
    //Mapping table
    uint32_t * mapping = malloc(MAX_SECTOR* sizeof(uint32_t));
    uint32_t vssd_pages = vssd_1->max_addr*1024;
    uint32_t * rev_mapping = malloc(vssd_pages * sizeof(uint32_t));
    memset(mapping, 0, MAX_SECTOR * sizeof(uint32_t));
    memset(rev_mapping, 0,vssd_pages * sizeof(uint32_t));
    thread_data_t thr_data;
    thr_data.vssd = vssd_1;
    thr_data.mapping = mapping;
    thr_data.rev_mapping = rev_mapping;
    thr_data.index = port;

    pthread_t net_thread;
    if ((rc = pthread_create(&net_thread, NULL, run_net, &port))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }
    pthread_t gc_thread;
    if ((rc = pthread_create(&gc_thread, NULL, run_gc, &thr_data))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }
    pthread_t worker_thread;
    if ((rc = pthread_create(&worker_thread, NULL, run_worker, &thr_data))) {
        fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
        return EXIT_FAILURE;
    }

    pthread_join(worker_thread, NULL);
    pthread_join(net_thread, NULL);
    pthread_join(gc_thread, NULL);

    free(buf);
    free(readbuf);
    free(metabuf);
    free(readmetabuf);
    free(mapping);
    
    return 0;

} 
