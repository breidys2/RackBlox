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


#define NUM_PKTS 2

//First trying just sending from one port to the other
char ip_client[][32] = {"192.168.2.3",};
char ip_server[][32] = {"192.168.2.1","192.168.2.3", "192.168.2.4"};

uint16_t src_port = 8888;
uint16_t dst_port = 1234;

uint64_t pkt_sent = 0;
uint64_t pkt_recv = 0;



/************* CUSTOM PARAMS **************/
uint32_t qps = 100000;

//Packet Related
uint32_t req_id = 0;
int max_req_id = (1 << 12);
uint32_t req_id_core[8] = {0};

/************* FUNCTIONS **************/

//Create the custom RackBlox Packet 
static void generate_packet(uint32_t lcore_id, struct rte_mbuf *mbuf, char* buffer, int buflen) {

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

    //Add the ip addresses  (inet_pton converts text to binary) (replaces filler from header_template)
    inet_pton(AF_INET, ip_client[0], &(ip->src_addr));
    inet_pton(AF_INET, ip_server[0], &(ip->dst_addr));
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);

    //RackBlox portion of packet forwarding 
    Message *req = (Message *)((uint8_t *)eth + sizeof(header_template));
    memcpy(req, buffer, buflen);
    
    mbuf->data_len += buflen;
    mbuf->pkt_len += buflen;
    printf("Sending to switch\n");
    printf("type: %d\n", req->type);
    printf("vssd_id: %u\n", req->vssd_id);
    printf("ing_lat: %u\n", req->ing_lat);
    //printf("ing_lat host: %lu\n", ntohul(req->ing_lat));
    printf("eg_lat: %u\n", req->eg_lat);
    //printf("eg_lat host: %lu\n", ntohul(req->eg_lat));
    printf("gen_ns: %lu\n", req->gen_ns);
    printf("addr: %u\n", req->addr);
    printf("================\n");
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
    //uint64_t reply_run_ns = rte_le_to_cpu_64(req->run_ns);
    //assert(cur_ns > req->gen_ns);
    
    printf("Recv from switch\n");
    printf("type: %d\n", req->type);
    printf("vssd_id: %u\n", req->vssd_id);
    printf("ing_lat: %u\n", req->ing_lat);
    //printf("ing_lat host: %lu\n", ntohul(req->ing_lat));
    printf("eg_lat: %u\n", req->eg_lat);
    //printf("eg_lat host: %lu\n", ntohul(req->eg_lat));
    printf("gen_ns: %lu\n", req->gen_ns);
    printf("addr: %u\n", req->addr);
    printf("================\n");

    //Do the net update
    req->lat = net_lats[pkt_recv] + abs(req->eg_lat - req->ing_lat);

    pkt_recv++;
    //Once done "processing" the packet, want to forward it to spr1
    char buf[MAXBUF];
    memcpy(buf, req, sizeof(Message));
    if (req->vssd_id == 1) {
        if (sendto(spr1_socket_1, buf, sizeof(Message), 0,
                    (struct sockaddr *)&spr1_1, sizeof(spr1_1)) < 0) {
            printf("Failed to send to spr1_1\n");
            exit(2);
        }
        printf("Forwarded packet to spr1_1\n");
    } else if (req->vssd_id == 2) {
        if (sendto(spr1_socket_2, buf, sizeof(Message), 0,
                    (struct sockaddr *)&spr1_2, sizeof(spr1_2)) < 0) {
            printf("Failed to send to spr1_2\n");
            exit(2);
        }
        printf("Forwarded packet to spr1_2\n");
    }
}

//Loop for receiving responses
static void rx_loop(uint32_t lcore_id) {

    //Debug info
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    printf("%lld entering RX loop on lcore %u\n", (long long) time(NULL), lcore_id);

    //Vars for later
    struct rte_mbuf *mbuf;
    struct rte_mbuf *mbuf_burst[MAX_BURST_SIZE];
    uint32_t i,j,nb_rx;
    uint32_t done = 0;

    //Infinite loop until pkt_lim reached
    while(!done) {
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
                //done = 1;
                //Free the mbuf, we are done with this packet
                rte_pktmbuf_free(mbuf);
            }
        }
        socklen_t len;
        int n;
        //Check spr1
        n = recvfrom(spr1_socket_1, buffer, MAXBUF, 0, ( struct sockaddr *) &cliaddr, &len);
        if (n > 0) {
            //Send packet through switch
            mbuf = rte_pktmbuf_alloc(pktmbuf_pool);
            generate_packet(lcore_id, mbuf, buffer, n);
            enqueue_pkt(lcore_id, mbuf);
            send_pkt_burst(lcore_id);
            printf("Forwarded to spr7: %d\n", n);
        }
    }
    printf("Done Processing Packets\n");
}

static int32_t client_loop(__attribute__((unused)) void *arg) {
    uint32_t lcore_id = rte_lcore_id();
    rx_loop(lcore_id);
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
    while((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stdout, "HELP TEXT: \n");
                parse_client_args_help(argv[0]);
                break;
            case 'p':
                enabled_port_mask = atoi(optarg);
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
    if (ret < 0)  {
        rte_exit(EXIT_FAILURE, "bad client args\n");
    }

    init();

	/* Launches the function on each lcore. 8< */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		/* Simpler equivalent. 8< */
		rte_eal_remote_launch(client_loop, NULL, lcore_id);
		/* >8 End of simpler equivalent. */
	}

	/* call it on main lcore too */
	client_loop(NULL);
	/* >8 End of launching the function on each lcore. */

	rte_eal_mp_wait_lcore();

    //Close udp socket for sending to spr1
    close(spr1_socket_1);
    close(spr1_socket_2);

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
