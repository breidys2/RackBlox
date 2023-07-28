#ifndef UTIL_H_
#define UTIL_H_

#include <time.h>
#include <unistd.h>

/* 
 * Constants
 */ 
#define CLIENT_PORT 8888
#define SERVICE_PORT 1234
//These are used as filler, but should be valid
#define IP_SRC "192.168.2.1"
#define IP_DST "192.168.2.2"
#define SPR1_PORT_1 8886
#define SPR1_PORT_2 8887
#define SPR1_IP "192.17.101.15"
#define PKT_LIM 44

#define MAX_LCORES 2
#define MAX_BURST_SIZE 32
#define RX_QUEUE_PER_LCORE 2
#define NB_RXD 256
#define NB_TXD 256

//!! This needs to be large enough to satisfy NB_RXD*RX_QUEUE_PER_PORT (depends on PER_LCORE and ports)
#define NB_MBUF 16384
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 32
#define MAXBUF 5120

#define TYPE_DEBUG 1

/* 
 * Global Vars
 */

struct sockaddr_in spr1_1;
struct sockaddr_in spr1_2;
struct sockaddr_in cliaddr;
int spr1_socket_1;
int spr1_socket_2;
char buffer[MAXBUF];
uint64_t net_lats[PKT_LIM];
struct rte_mempool *pktmbuf_pool = NULL;
struct rte_ether_addr port_eth_addrs[RTE_MAX_ETHPORTS];
uint32_t n_enabled_ports = 0;
uint32_t enabled_port_mask = 1;
uint32_t enabled_ports[RTE_MAX_ETHPORTS];
uint32_t n_lcores = 0;

/********* Misc *********/

// Get current time in ns
uint64_t get_cur_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
    return t;
}

// Functions for Initialization
uint8_t header_template[sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr)];



struct mbuf_table {
    uint32_t len;
    struct rte_mbuf *m_table[MAX_BURST_SIZE];
};

//Useful stuff for managing the lcores
struct lcore_configuration {
    uint32_t vid;                                // virtual core id
    uint32_t port;                               // one port
    uint32_t tx_queue_id;                        // one TX queue
    uint32_t n_rx_queue;                         // number of RX queues
    uint32_t rx_queue_list[RX_QUEUE_PER_LCORE];  // list of RX queues
    struct mbuf_table tx_mbufs;                  // mbufs to hold TX queue
} __rte_cache_aligned;

struct lcore_configuration lcore_conf[MAX_LCORES];

// send packets, drain TX queue
static void send_pkt_burst(uint32_t lcore_id) {
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    //Get the m_table of the mbufs ready to be sent
    struct rte_mbuf **m_table = (struct rte_mbuf **)lconf->tx_mbufs.m_table;

    //Get the number of packets to send
    uint32_t n = lconf->tx_mbufs.len;
    //Actually send the burst
    uint32_t ret = rte_eth_tx_burst(lconf->port, lconf->tx_queue_id, m_table,
            lconf->tx_mbufs.len);
    //If not send all, free the ones that weren't sent
    //Presumably the tx_burst function frees those that are correctly sent
    if (unlikely(ret < n)) {
        do {
            rte_pktmbuf_free(m_table[ret]);
        } while (++ret < n);
    }
    //Reset the length
    lconf->tx_mbufs.len = 0;
}

static const struct rte_eth_conf port_conf = {
    .rxmode =
    {
        //.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
        //.split_hdr_size = 0,
        //.header_split = 0,    // Header Split disabled
        //.hw_ip_checksum = 0,  // IP checksum offload disabled
        //.hw_vlan_filter = 0,  // VLAN filtering disabled
        //.jumbo_frame = 0,     // Jumbo Frame Support disabled
        //.hw_strip_crc = 0,    // CRC stripped by hardware
    },
    .txmode =
    {
        .mq_mode = ETH_MQ_TX_NONE,
    },
};

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
    uint32_t pad1;
    uint32_t ing_lat;
    uint32_t pad2;
    uint32_t eg_lat;
    uint64_t gen_ns;
    uint32_t addr;
    uint64_t lat;
} __attribute__((__packed__)) Message;

typedef struct MessageReply_ {
    Message msg;
    char data[4096];
} MessageReply;

// put packet into TX queue
static void enqueue_pkt(uint32_t lcore_id, struct rte_mbuf *mbuf) {
  struct lcore_configuration *lconf = &lcore_conf[lcore_id];
  //Packets stored in this mbuf table
  lconf->tx_mbufs.m_table[lconf->tx_mbufs.len++] = mbuf;

  // enough packets in TX queue
  if (unlikely(lconf->tx_mbufs.len == MAX_BURST_SIZE)) {
    send_pkt_burst(lcore_id);
  }
}

// check link status
static void check_link_status(void) {
    printf("Entering check_link_status\n");
    const uint32_t check_interval_ms = 100;
    const uint32_t check_iterations = 90;
    uint32_t i, j;
    struct rte_eth_link link;
    for (i = 0; i < check_iterations; i++) {
        uint8_t all_ports_up = 1;
        for (j = 0; j < n_enabled_ports; j++) {
            uint32_t portid = enabled_ports[j];
            memset(&link, 0, sizeof(link));
            rte_eth_link_get_nowait(portid, &link);
            if (link.link_status) {
                printf("\tport %u link up - speed %u Mbps - %s\n", portid,
                        link.link_speed,
                        (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? "full-duplex"
                        : "half-duplex");
            } else {
                all_ports_up = 0;
            }
        }

        if (all_ports_up == 1) {
            printf("check link status finish: all ports are up\n");
            break;
        } else if (i == check_iterations - 1) {
            printf("check link status finish: not all ports are up\n");
        } else {
            rte_delay_ms(check_interval_ms);
        }
    }
}


// init header template
static void init_header_template(void) {
    //Zero out the memory
    memset(header_template, 0, sizeof(header_template));
    //Allocate the correct structs at the correct offsets
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)header_template;
    struct rte_ipv4_hdr *ip =
        (struct rte_ipv4_hdr *)((uint8_t *)eth + sizeof(struct rte_ether_hdr));
    struct rte_udp_hdr *udp =
        (struct rte_udp_hdr *)((uint8_t *)ip + sizeof(struct rte_ipv4_hdr));
    //Set the correct packet length
    uint32_t pkt_len = sizeof(header_template) + sizeof(Message);

    // eth header
    // Using an endian convertion here (to BigEndian)
    eth->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    //These are filler
    struct rte_ether_addr src_addr = {
        .addr_bytes = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};
    //These are filler
    struct rte_ether_addr dst_addr = {
        .addr_bytes = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    //Copy the addresses over
    rte_ether_addr_copy(&src_addr, &eth->src_addr);
    rte_ether_addr_copy(&dst_addr, &eth->dst_addr);

    // ip headers (these get replaced)
    char src_ip[] = IP_SRC;
    char dst_ip[] = IP_DST;
    //Converts text ip address (src/dst) to binary in the ip object
    int st1 = inet_pton(AF_INET, src_ip, &(ip->src_addr));
    int st2 = inet_pton(AF_INET, dst_ip, &(ip->dst_addr));
    if (st1 != 1 || st2 != 1) {
        fprintf(stderr, "inet_pton() failed.Error message: %s %s", strerror(st1),
                strerror(st2));
        exit(EXIT_FAILURE);
    }
    //Set various IP hdr fields
    //Adjust the length (again another endian conversion)
    ip->total_length = rte_cpu_to_be_16(pkt_len - sizeof(struct rte_ether_hdr));
    //version (setting it to ipv4)
    //ihl = internet header length denoting length of header field in 32 bit increments (setting to 5)
    ip->version_ihl = 0x45;
    //a.k.a differentiated services code point (DSCP) (unrelated here, used for data streaming)
    ip->type_of_service = 0;
    //Used to identify fragments (probably filler here)
    ip->packet_id = 0;
    //Number of data bytes ahead of a particular fragment (depends on flags that we don't set)
    ip->fragment_offset = 0;
    //Set to 64, a.k.a don't worry about it.
    ip->time_to_live = 64;
    //Used to indicate tcp vs. udp (we use udp)
    ip->next_proto_id = IPPROTO_UDP;
    //Header checksum computed here
    uint32_t ip_cksum;
    uint16_t *ptr16 = (uint16_t *)ip;
    ip_cksum = 0;
    ip_cksum += ptr16[0];
    ip_cksum += ptr16[1];
    ip_cksum += ptr16[2];
    ip_cksum += ptr16[3];
    ip_cksum += ptr16[4];
    ip_cksum += ptr16[6];
    ip_cksum += ptr16[7];
    ip_cksum += ptr16[8];
    ip_cksum += ptr16[9];
    ip_cksum = ((ip_cksum & 0xffff0000) >> 16) + (ip_cksum & 0x0000ffff);
    if (ip_cksum > 65535) {
        ip_cksum -= 65535;
    }
    ip_cksum = (~ip_cksum) & 0x0000ffff;
    if (ip_cksum == 0) {
        ip_cksum = 0xffff;
    }
    ip->hdr_checksum = (uint16_t)ip_cksum;

    // udp header
    // htons converts host byte order to network byte order
    udp->src_port = htons(CLIENT_PORT);
    udp->dst_port = htons(SERVICE_PORT);
    udp->dgram_len = rte_cpu_to_be_16(pkt_len - sizeof(struct rte_ether_hdr) -
            sizeof(struct rte_ipv4_hdr));
    udp->dgram_cksum = 0;
}


static void init(void) {
    uint32_t i, j;

    // create mbuf pool
    printf("create mbuf pool\n");
    pktmbuf_pool = rte_mempool_create(
            "mbuf_pool", NB_MBUF, MBUF_SIZE, MBUF_CACHE_SIZE,
            sizeof(struct rte_pktmbuf_pool_private), rte_pktmbuf_pool_init, NULL,
            rte_pktmbuf_init, NULL, rte_socket_id(), 0);
    if (pktmbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "cannot init mbuf pool\n");
    }

    // determine available ports
    printf("create enabled ports\n");
    uint32_t n_total_ports = 0;
    n_total_ports = rte_eth_dev_count_avail();

    if (n_total_ports == 0) {
        rte_exit(EXIT_FAILURE, "cannot detect ethernet ports\n");
    }
    if (n_total_ports > RTE_MAX_ETHPORTS) {
        n_total_ports = RTE_MAX_ETHPORTS;
    }

    // get info for each enabled port
    struct rte_eth_dev_info dev_info;
    n_enabled_ports = 0;
    printf("\tports: ");
    for (i = 0; i < n_total_ports; i++) {
        if ((enabled_port_mask & (1 << i)) == 0) {
            continue;
        }
        enabled_ports[n_enabled_ports++] = i;
        rte_eth_dev_info_get(i, &dev_info);
        printf("%u ", i);
    }
    printf("\n");

    // find number of active lcores
    printf("create enabled cores\n\tcores: ");
    n_lcores = 0;
    for (i = 0; i < MAX_LCORES; i++) {
        if (rte_lcore_is_enabled(i)) {
            n_lcores++;
            printf("%u ", i);
        }
    }
    printf("\n");
    
    // ensure numbers are correct
    printf("%d n_lcores, %d n_enabled_ports\n", n_lcores, n_enabled_ports);
    if (n_lcores % n_enabled_ports != 0) {
        rte_exit(EXIT_FAILURE,
                "number of cores (%u) must be multiple of ports (%u)\n", n_lcores,
                n_enabled_ports);
    }

    //Receive queues per logical core
    uint32_t rx_queues_per_lcore = RX_QUEUE_PER_LCORE;
    //Map those receive queues to ports based on lcore -> port mapping
    uint32_t rx_queues_per_port =
        rx_queues_per_lcore * n_lcores / n_enabled_ports;
    uint32_t tx_queues_per_port = n_lcores / n_enabled_ports;

    //Sanity check
    if (rx_queues_per_port < rx_queues_per_lcore) {
        rte_exit(EXIT_FAILURE,
                "rx_queues_per_port (%u) must be >= rx_queues_per_lcore (%u)\n",
                rx_queues_per_port, rx_queues_per_lcore);
    }

    // assign each lcore some RX queues and a port
    printf("set up %d RX queues per port and %d TX queues per port\n",
            rx_queues_per_port, tx_queues_per_port);
    uint32_t portid_offset = 0;
    uint32_t rx_queue_id = 0;
    uint32_t tx_queue_id = 0;
    //vid = "virtual id"
    uint32_t vid = 0;
    for (i = 0; i < MAX_LCORES; i++) {
        if (rte_lcore_is_enabled(i)) {
            lcore_conf[i].vid = vid++;
            //number of rx queues
            lcore_conf[i].n_rx_queue = rx_queues_per_lcore;
            //assign all the lcores to the port (we are doing one to one)
            for (j = 0; j < rx_queues_per_lcore; j++) {
                lcore_conf[i].rx_queue_list[j] = rx_queue_id++;
            }
            //Add the port
            lcore_conf[i].port = enabled_ports[portid_offset];
            //Give the tx queue
            lcore_conf[i].tx_queue_id = tx_queue_id++;
            //Moving to a new port
            if (rx_queue_id % rx_queues_per_port == 0) {
                portid_offset++;
                rx_queue_id = 0;
                tx_queue_id = 0;
            }
        }
    }

    printf("Moving on to port init\n");
    // initialize each port
    for (portid_offset = 0; portid_offset < n_enabled_ports; portid_offset++) {
        uint32_t portid = enabled_ports[portid_offset];

        int32_t ret = rte_eth_dev_configure(portid, rx_queues_per_port,
                tx_queues_per_port, &port_conf);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "cannot configure device: err=%d, port=%u\n", ret,
                    portid);
        }
        rte_eth_macaddr_get(portid, &port_eth_addrs[portid]);

        // initialize RX queues
        for (i = 0; i < rx_queues_per_port; i++) {
            ret = rte_eth_rx_queue_setup(
                    portid, i, NB_RXD, rte_eth_dev_socket_id(portid), NULL, pktmbuf_pool);
            if (ret < 0) {
                rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup: err=%d, port=%u\n", ret,
                        portid);
            }
        }

        // initialize TX queues
        for (i = 0; i < tx_queues_per_port; i++) {
            ret = rte_eth_tx_queue_setup(portid, i, NB_TXD,
                    rte_eth_dev_socket_id(portid), NULL);
            if (ret < 0) {
                rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup: err=%d, port=%u\n", ret,
                        portid);
            }
        }

        // start device
        ret = rte_eth_dev_start(portid);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "rte_eth_dev_start: err=%d, port=%u\n", ret,
                    portid);
        }

        rte_eth_promiscuous_enable(portid);

        char mac_buf[RTE_ETHER_ADDR_FMT_SIZE];
        rte_ether_format_addr(mac_buf, RTE_ETHER_ADDR_FMT_SIZE, &port_eth_addrs[portid]);
        printf("initiaze queues and start port %u, MAC address:%s\n", portid,
                mac_buf);


        //
    }

    if (!n_enabled_ports) {
        rte_exit(EXIT_FAILURE,
                "all available ports are disabled. Please set portmask.\n");
    }
    check_link_status();
    init_header_template();
    printf("Setting up udp client to spr1\n");
    unsigned short port;
    char buf[32];

    // Set up vssd1 
    port = htons(SPR1_PORT_1);
    if ((spr1_socket_1 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket() call failed\n");
        exit(1);
    }
    spr1_1.sin_family      = AF_INET;  
    spr1_1.sin_port        = port;    
    spr1_1.sin_addr.s_addr = inet_addr(SPR1_IP); 
    //Set up vssd2
    port = htons(SPR1_PORT_2);
    if ((spr1_socket_2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket() call failed\n");
        exit(1);
    }
    spr1_2.sin_family      = AF_INET;  
    spr1_2.sin_port        = port;    
    spr1_2.sin_addr.s_addr = inet_addr(SPR1_IP); 

    //Set read timeout for later
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 10;
    setsockopt(spr1_socket_1, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    setsockopt(spr1_socket_2, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
    memset(&cliaddr, 0, sizeof(cliaddr));


    //Parse the net lats
    FILE *fp;
    int ret;
    fp = fopen("net_trace.dat","r");
    for (int i = 0; i < PKT_LIM; i++) {
        ret = fscanf(fp, "%lu", &net_lats[i]);
        ////For this testing, take one way -> rtt
        //net_lats[i] *= 2;
        //For this, take us -> ns
        net_lats[i] *= 1000;
        if (ret <= 0) {
            printf("Failed to scan network trace\n");
            exit(-1);
        }
    }
    fclose(fp);
}


#endif //UTIL_H_
