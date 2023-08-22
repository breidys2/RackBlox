/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <assert.h>
#include <inttypes.h>

#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_per_lcore.h>
#include <rte_udp.h>

#include "util.h"


#define NUM_PKTS 1

//First trying just sending from one port to the other
//TODO These are placeholders, replace with your own client/server IPs
char ip_client[][32] = {"192.168.2.1",};
char ip_server[][32] = {"192.168.2.3", "192.168.2.4"};

//Custom port for RackBlox, checked by switch
uint16_t src_port = 8888;
//Unused
uint16_t dst_port = 1234;

uint64_t pkt_sent = 0;
uint64_t pkt_recv = 0;


/************* CUSTOM PARAMS **************/
uint32_t qps = 1000;

//Packet Related
uint32_t req_id = 0;
int max_req_id = (1 << 12);
uint32_t req_id_core[8] = {0};

/************* FUNCTIONS **************/

//Create the custom RackBlox Packet 
static void generate_packet(uint32_t lcore_id, struct rte_mbuf *mbuf, uint64_t gen_ns, int pkt_id) {

    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    if (pkt_sent == 0) {
        printf("sending from %u\n", lconf->port);
    }
    assert(mbuf != NULL);
    //Use in case of scattered packet (this is not)
    mbuf->next = NULL;
    //Number of segments 
    mbuf->nb_segs = 1;
    //Offload flags (not using any)
    mbuf->ol_flags = 0;
    //Amount of data in the segment buffer
    mbuf->data_len = 0;
    //Packet length (sum of all segments)
    mbuf->pkt_len = 0;
    
    //Init the packet header
    //Returns a pointer to the start of the data in mbuf, cast to type ether_hdr*
    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    //Pointer arithmetic to get the head of the ipv4 header in the packet
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)((uint8_t *)eth + sizeof(struct rte_ether_hdr));
    //Pointer arithmetic to get the head of the udp header in the packet
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)((uint8_t *)ip + sizeof(struct rte_ipv4_hdr));
    //Copy in our header_template (?)
    rte_memcpy(eth, header_template, sizeof(header_template));
    //Update mbuf values accordingly
    mbuf->data_len += sizeof(header_template);
    mbuf->pkt_len += sizeof(header_template);

    //Init the custom aspect of the packet (Message) (appended after the udp header)
    Message *req = (Message *)((uint8_t *)eth + sizeof(header_template));
    
    //Add the ip addresses  (inet_pton converts text to binary) (replaces filler from header_template)
    inet_pton(AF_INET, ip_client[0], &(ip->src_addr));
    //printf("Src Port %d\n", src_port);
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    

    //Update some stuff in Message
    StorageReq cur_req = trace_reqs[pkt_id];
    req->type = cur_req.RW;
    if (req->type == rb_write) {
        memset(req->data, 'y', DATA_SZ);
    }
    req->addr = cur_req.Addr;
    //For now, always select vssd 1
    req->vssd_id = cur_req.vssd_id;
    if (req->vssd_id == 0) {
        req->vssd_id = (rand() % 2) + 1;
    }
    inet_pton(AF_INET, ip_server[req->vssd_id-1], &(ip->dst_addr));
    req->ing_lat = 0;
    req->eg_lat = 0;
    req->gen_ns = gen_ns;
    
    printf("Sending Packet for:\n");
    printf("type: %d\n", req->type);
    printf("vssd_id: %u\n", req->vssd_id);
    printf("ing_lat: %lu\n", req->ing_lat);
    printf("eg_lat: %lu\n", req->eg_lat);
    printf("gen_ns: %lu\n", req->gen_ns);
    printf("addr: %u\n", req->addr);
    //printf("data preview %c\n", req->data[0]);
    printf("================\n");

    //Update with the addition of our custom stuff
    mbuf->data_len += sizeof(Message);
    mbuf->pkt_len += sizeof(Message);

}


//Process the custom packet
static void process_packet(uint32_t lcore_id, struct rte_mbuf *mbuf) {
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];

    //parse the packet header
    //Returns a pointer to the start of the data in mbuf, cast to type ether_hdr*
    struct rte_ether_hdr *eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)((uint8_t *)eth + sizeof(struct rte_ether_hdr));
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)((uint8_t *)ip + sizeof(struct rte_ipv4_hdr));

    //Parse our custom header
    Message *req = (Message *)((uint8_t *)eth + sizeof(header_template));

    //debug
    uint64_t cur_ns = get_cur_ns();
    uint16_t reply_port = ntohs(udp->src_port);
    req_latencies[pkt_recv++] = cur_ns - req->gen_ns;
    //uint64_t reply_run_ns = rte_le_to_cpu_64(req->run_ns);
    //assert(cur_ns > req->gen_ns);
    
    printf("Receiving Packet %d from:\n", pkt_recv);
    printf("type: %d\n", req->type);
    printf("vssd_id: %u\n", req->vssd_id);
    printf("ing_lat: %lu\n", req->ing_lat);
    printf("eg_lat: %lu\n", req->eg_lat);
    printf("gen_ns: %lu\n", req->gen_ns);
    printf("addr: %u\n", req->addr);
    printf("lat: %lu\n", req->lat);
    printf("data: %c\n", req->data[0]);
    printf("================\n");
}

//Loop for receiving responses
static void rx_loop(uint32_t lcore_id) {

    //Debug info
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    printf("%lld entering RX loop (master loop) on lcore %u\n", (long long) time(NULL), lcore_id);

    //Vars for later
    struct rte_mbuf *mbuf;
    struct rte_mbuf *mbuf_burst[MAX_BURST_SIZE];
    uint32_t i,j,nb_rx;
    uint32_t done = 0;

    //Infinite loop
    while(pkt_recv < reply_num) {
        //Iterate over the rx queues for the core
        for (i = 0; i < lconf->n_rx_queue; i++) {
            //Receive a burst
            nb_rx = rte_eth_rx_burst(lconf->port, lconf->rx_queue_list[i], mbuf_burst, MAX_BURST_SIZE);
            for (j = 0; j < nb_rx; j++) {
                //Get packet of the returned burst
                mbuf = mbuf_burst[j];
                //Performance prefetching of the pointer into the packet data (used in process packet)
                rte_prefetch0(rte_pktmbuf_mtod(mbuf, void*));
                //Process the packet
                process_packet(lcore_id, mbuf);
                //if (pkt_recv >= reply_num) done = 1;
                //Free the mbuf, we are done with this packet
                rte_pktmbuf_free(mbuf);
            }
        }
    }
    print_req_latencies();
    //free(req_latencies);
    printf("Packet Process Limit Reached\n");
}

//Fixed loop sending requests with exp distribution
static void fixed_tx_loop(uint32_t lcore_id) {
    struct lcore_configuration *lconf =&lcore_conf[lcore_id];
    printf("%lld entering fixed TX loop on lcore %u\n", (long long) time(NULL), lcore_id);
    
    struct rte_mbuf *mbuf;

    //Generating requests from an exponential distribution
    ExpDist *dist = malloc(sizeof(ExpDist));
    double lambda = qps * 1e-9;
    double mu = 1.0 / lambda;
    init_exp_dist(dist, mu);

    printf("lcore %u start sending fixed packets in %" PRIu32 " qps\n", lcore_id,
            qps);

    while (pkt_sent < req_num) {
        uint64_t gen_ns = exp_dist_next_arrival_ns(dist);
        uint32_t pkts_length = NUM_PKTS * sizeof(Message);
        for (uint8_t i = 0; i < NUM_PKTS; i++) {
            mbuf = rte_pktmbuf_alloc(pktmbuf_pool);
            generate_packet(lcore_id, mbuf, gen_ns, pkt_sent);
            enqueue_pkt(lcore_id, mbuf);
        }
        req_id++;
        if (req_id_core[lcore_id] >= max_req_id) {
            req_id_core[lcore_id] = 0;
        } else {
            req_id_core[lcore_id]++;
        }
        while (get_cur_ns() < gen_ns)
            ;
        send_pkt_burst(lcore_id);
        pkt_sent++;
    }
    printf("Sent all packets!\n");
    free_exp_dist(dist);
}

static void tx_loop(uint32_t lcore_id) {
    //For now, just redirect
    fixed_tx_loop(lcore_id);
}

static int32_t client_loop(__attribute__((unused)) void *arg) {
    uint32_t lcore_id = rte_lcore_id();
    if (lcore_id == 1) {
        tx_loop(lcore_id);
    } else {
        rx_loop(lcore_id);
    }
    return 0;
}

static void parse_client_args_help(char *program_name) {
    fprintf(stderr, 
            "Usage: %s"
            "-h --> Print the help message "
            "-p --> Updates the port mask (default = 1)"
            "\n",
            program_name);
}

static int parse_client_args(int argc, char **argv) {
    int opt, num;
    while((opt = getopt(argc, argv, "p:n:t:r:h")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stdout, "HELP TEXT: \n");
                parse_client_args_help(argv[0]);
                break;
            case 'p':
                enabled_port_mask = atoi(optarg);
                break;
            case 'n':
                req_num = atoi(optarg);
                break;
            case 'r':
                reply_num = atoi(optarg);
                break;
            case 't':
                strcpy(trace_name, optarg);
                break;
            default:
                parse_client_args_help(argv[0]);
                return -1;
        }
    }
    return 0;
}


/* Initialization of Environment Abstraction Layer (EAL). 8< */
int
main(int argc, char **argv)
{
	int ret;
	unsigned lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");
	/* >8 End of initialization of Environment Abstraction Layer */

    //Remove the eal related args 
    argc -= ret;
    argv += ret;

    ret = parse_client_args(argc, argv);
    req_latencies = (uint64_t*)malloc(reply_num * sizeof(uint64_t));
    if (ret < 0)  {
        rte_exit(EXIT_FAILURE, "bad client args\n");
    }

    init();
    trace_reqs = parse_trace(trace_name, req_num);

    //!! Commenting out here since we only run one client
	/* Launches the function on each lcore. 8< */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
	   /* Simpler equivalent. 8< */
	   rte_eal_remote_launch(client_loop, NULL, lcore_id);
	   /* >8 End of simpler equivalent. */
	}
    
    //Collect the trace

	/* call it on main lcore too */
	client_loop(NULL);
	/* >8 End of launching the function on each lcore. */

	rte_eal_mp_wait_lcore();

	/* clean up the EAL */
	rte_eal_cleanup();
    free(trace_reqs);

	return 0;
}
