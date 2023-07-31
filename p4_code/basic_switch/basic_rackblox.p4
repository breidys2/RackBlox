#include <core.p4>
#include <tna.p4>

#include "../includes/headers.p4"
#include "../includes/util.p4"
#include "../includes/constants.p4"


struct metadata_t {
    bit<1> gc_vssd;
    bit<1> gc_replica;
    bit<32> replica_id;
    ipv4_addr_t r_ip;
}

// ---------------------------------------------------------------------------
// Ingress parser
// ---------------------------------------------------------------------------
parser SwitchIngressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {

    TofinoIngressParser() tofino_parser;
    state start {
        tofino_parser.apply(pkt, ig_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);

        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            default : reject;
        }
    }

    //Parse the udp header and move on to custom header
    state parse_udp {
        pkt.extract(hdr.udp);

        //Check for custom port num
        transition select(hdr.udp.src_port) {
            RB_PORT: parse_rb;
            default: accept;
        }
    }
    //Parse the netflash header
    state parse_rb {
        pkt.extract(hdr.rb);
        ig_md.gc_vssd = 0;
        ig_md.gc_replica = 0;
        ig_md.r_ip = 0;
        //Done parsing, move to accept
        transition accept;
    }
}

// ---------------------------------------------------------------------------
// Ingress
// ---------------------------------------------------------------------------
control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {

    action update_lat_ingress() {
        //hdr.rb.lat1 = (bit<64>) ig_intr_md.ingress_mac_tstamp;
        hdr.rb.lat1 = (bit<64>) ig_prsr_md.global_tstamp;
    }
    table lat_ingress_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            update_lat_ingress;
        }
        default_action = update_lat_ingress();
        size = TABLE_LENGTH;
    }
    action set_output_port(PortId_t egress_port) {
        ig_tm_md.ucast_egress_port = egress_port;
    }
    //Create indirect registers
    Register<bit<1>, bit<1>>(REGISTER_LENGTH, 0) replica_registers;

    RegisterAction<bit<1>, _, void>(replica_registers) zero_replica = {
        void apply(inout bit<1> value) {
            value = 0;
        }
    };
    RegisterAction<_, _, bit<1>>(replica_registers) read_replica = {
        void apply(inout bit<1> value, out bit<1> res) {
            res = value;
        }
    };
    RegisterAction<_, _, void>(replica_registers) one_replica = {
        void apply(inout bit<1> value) {
            value = 1;
        }
    };
    action set_gc_replica(bit<32> idx) {
        one_replica.execute(idx);
    }
    table set_gc_replica_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            set_gc_replica;
        }
        default_action = set_gc_replica(0);
        const entries = {
            (0x1) : set_gc_replica(1);
            (0x2) : set_gc_replica(2);
        }
        size = TABLE_LENGTH;
    }

    action unset_gc_replica(bit<32> idx) {
        zero_replica.execute(idx);
    }
    table unset_gc_replica_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            unset_gc_replica;
        }
        default_action = unset_gc_replica(0);
        const entries = {
            (0x1) : unset_gc_replica(1);
            (0x2) : unset_gc_replica(2);
        }
        size = TABLE_LENGTH;
    }

    Register<bit<1>, bit<1>>(REGISTER_LENGTH, 0) destination_registers;
    RegisterAction<bit<1>, _, void>(destination_registers) zero_destination = {
        void apply(inout bit<1> value) {
            value = 0;
        }
    };
    RegisterAction<_, _, bit<1>>(destination_registers) read_destination = {
        void apply(inout bit<1> value, out bit<1> res) {
            res = value;
        }
    };
    RegisterAction<_, _, void>(destination_registers) one_destination = {
        void apply(inout bit<1> value) {
            value = 1;
        }
    };
    action set_gc_destination(bit<32> idx) {
        one_destination.execute(idx);
    }
    table set_gc_destination_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            set_gc_destination;
        }
        default_action = set_gc_destination(0);
        const entries = {
            (0x1) : set_gc_destination(1);
            (0x2) : set_gc_destination(2);
        }
        size = TABLE_LENGTH;
    }

    action unset_gc_destination(bit<32> idx) {
        zero_destination.execute(idx);
    }
    table unset_gc_destination_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            unset_gc_destination;
        }
        default_action = unset_gc_destination(0);
        const entries = {
            (0x1) : unset_gc_destination(1);
            (0x2) : unset_gc_destination(2);
        }
        size = TABLE_LENGTH;
    }

    action read_replica_table(bit<32> replica) {
        ig_md.replica_id = replica;
        ig_md.gc_vssd = read_replica.execute(hdr.rb.vssd_id);
    }
    action read_destination_table(ipv4_addr_t r_ip) {
        ig_md.r_ip = r_ip;
        ig_md.gc_replica = read_destination.execute(ig_md.replica_id);
    }
    table reply_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            set_output_port;
        }
        default_action = set_output_port(292);
        size = TABLE_LENGTH;
    }

    table redirect_port_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            set_output_port;
        }
        //Default action is to send out of port 6 (sprlab007's port 1)
        //Requests will always be coming from sprlab007's port 0
        default_action = set_output_port(300);
        const entries = {
            //Mapping vssd_id = 0 -> sprlab007 port 1 (port 300)
            (0x0) : set_output_port(300);
            //Mapping vssd_id = 1 -> sprlab008 port 0 (port 276)
            (0x1) : set_output_port(276);
            //Mapping vssd_id = 2 -> sprlab008 port 1 (port 284)
            (0x2) : set_output_port(284);
            //For debug
            (0x5) : set_output_port(276);
        }
        size = TABLE_LENGTH;
    }

    //First table described in the paper
    table replica_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            read_replica_table;
        }
        //Default action is to send out of port 6 (sprlab007's port 1)
        //Requests will always be coming from sprlab007's port 0
        //default_action = read_replica_table(0);
        const entries = {
            //This one is filler, probably can remove
            (0x0) : read_replica_table(0);
            //Go from vSSD 1->2
            (0x1) : read_replica_table(2);
            //Go from vSSD 2->1
            (0x2) : read_replica_table(1);
        }
        size = TABLE_LENGTH;
    }
    
    //Second table described in the paper
    table destination_table {
        key = {
            ig_md.replica_id: exact;
        }
        actions = {
            read_destination_table;
        }
        //Default action is to send out of port 6 (sprlab007's port 1)
        //Requests will always be coming from sprlab007's port 0
        //default_action = read_destination_table(3232236034);
        const entries = {
            //Mapping vssd_id = 0 -> sprlab007 port 1 (192.168.2.2)
            (0x0) : read_destination_table(3232236034);
            //Mapping vssd_id = 1 -> sprlab008 port 0 (192.168.2.3)
            (0x1) : read_destination_table(3232236035);
            //Mapping vssd_id = 2 -> sprlab008 port 1 (192.168.2.4)
            (0x2) : read_destination_table(3232236036);
        }
        size = TABLE_LENGTH;
    }
    action redirect_read() {
        //Change output port
        hdr.rb.vssd_id = ig_md.replica_id;
        //Change dst ip address
        hdr.ipv4.dst_addr = ig_md.r_ip;
    }

    //Table used for read redirection
    table read_redirect_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            redirect_read;
        }
        default_action = redirect_read();
        size = TABLE_LENGTH;
    }

    action send_success() {
        hdr.rb.op = TYPE_GC_SUCCESS;
    }
    table gc_success_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            send_success;
        }
        default_action = send_success();
        size = TABLE_LENGTH;
    }
    action send_finish_success() {
        hdr.rb.op = TYPE_GC_FIN_SUC;
    }
    table gc_fin_suc_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            send_finish_success;
        }
        default_action = send_finish_success();
        size = TABLE_LENGTH;
    }

    action send_deny() {
        hdr.rb.op = TYPE_GC_DENY;
    }
    table gc_deny_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            send_deny;
        }
        default_action = send_deny();
        size = TABLE_LENGTH;
    }

    action set_gc() {
        hdr.rb.op = TYPE_GC;
    }
    table set_gc_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            set_gc;
        }
        default_action = set_gc();
        size = TABLE_LENGTH;
    }

    action send_back() {
        bit<48> tmp;

        // Swap the MAC addresses
        tmp = hdr.ethernet.dst_addr;
        hdr.ethernet.dst_addr = hdr.ethernet.src_addr;
        hdr.ethernet.src_addr = tmp;

        // Send the packet back to the port it came from 
        ig_tm_md.ucast_egress_port = ig_intr_md.ingress_port;

    }
    table send_back_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            send_back;
        }
        default_action = send_back();
        size = TABLE_LENGTH;
    }

    action recirc() {
        // Send the packet back to the port it came from 
        ig_tm_md.ucast_egress_port = RECIRC_PORT;

    }
    table recirc_table {
        key = {
            ig_intr_md.ingress_port: exact;
        }
        actions = {
            recirc;
        }
        default_action = recirc();
        size = TABLE_LENGTH;
    }
    
    //CORE LOGIC
    apply {
        if (hdr.rb.op == TYPE_WRITE) {
            //Do nothing, packet forwarded below
            redirect_port_table.apply();
        }
        else if (hdr.rb.op == TYPE_WRITE_REPLY) {
            //Do nothing, packet forwarded below
            reply_table.apply();
        }
        else if (hdr.rb.op == TYPE_READ || hdr.rb.op == TYPE_GC_REQUEST) {
            //Forward with possible redirection
            //Read the first table
            replica_table.apply();
            //Read the second table
            destination_table.apply();
            //Make routing decision
            if (hdr.rb.op == TYPE_READ) {
                if (ig_md.gc_vssd == 1 && ig_md.gc_replica == 0) {
                    read_redirect_table.apply();
                }
                redirect_port_table.apply();
            } else {
                if (ig_md.gc_replica == 0) {
                    //Change to TYPE_GC 
                    set_gc_table.apply();
                    //Recirculate
                    recirc_table.apply();
                } else {
                    //Send back deny packet
                    gc_deny_table.apply();
                    send_back_table.apply();
                    redirect_port_table.apply();
                }
            }
        }
        else if(hdr.rb.op == TYPE_READ_REPLY) {
            //Do nothing, packet forwarded below
            reply_table.apply();
        }
        else if(hdr.rb.op == TYPE_GC) {
            //Set the GC bit in the replica table
            set_gc_replica_table.apply();
            //Set the GC bit in the destination table
            set_gc_destination_table.apply();
            //Set the packet reply type
            gc_success_table.apply();
            //Then bounce packet back to sending server
            send_back_table.apply();
            redirect_port_table.apply();
        }
        else if(hdr.rb.op == TYPE_GC_FINISH) {
            //Set the GC bit in the replica table
            unset_gc_replica_table.apply();
            //Set the GC bit in the destination table
            unset_gc_destination_table.apply();
            //Set the packet reply type
            gc_fin_suc_table.apply();
            //Then bounce packet back to sending server
            send_back_table.apply();
            redirect_port_table.apply();
        }
        //Route the packet based on the target vssd_id
        lat_ingress_table.apply();
    }
}

// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchIngressDeparser(packet_out pkt,
                              inout header_t hdr,
                              in metadata_t ig_md,
                              in ingress_intrinsic_metadata_for_deparser_t 
                                ig_intr_dprsr_md
                              ) {


    apply {
        pkt.emit(hdr);
    }
}

// ---------------------------------------------------------------------------
// Egress Parser
// ---------------------------------------------------------------------------
parser SwitchEgressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {

    TofinoEgressParser() tofino_parser;

    state start {
        tofino_parser.apply(pkt, eg_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            default : reject;
        }
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        //Check for custom port num
        transition select(hdr.udp.src_port) {
            RB_PORT: parse_rb;
            default: accept;
        }
    }
    state parse_rb {
        pkt.extract(hdr.rb);
        //Done parsing, move to accept
        transition accept;
    }

}


// ---------------------------------------------------------------------------
// Egress 
// ---------------------------------------------------------------------------
control SwitchEgress(
        inout header_t hdr,
        inout metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_md_for_oport) {
    

    action update_lat_egress() {
        hdr.rb.lat2 = (bit<64>)eg_intr_from_prsr.global_tstamp;
    }
    table egress_lat_table {
        key = {
            hdr.rb.vssd_id: exact;
        }
        actions = {
            update_lat_egress;
        }
        default_action = update_lat_egress();
        size = TABLE_LENGTH;
    }
        
    apply {
        egress_lat_table.apply();
    }
}

// ---------------------------------------------------------------------------
// Egress Deparser
// ---------------------------------------------------------------------------
control SwitchEgressDeparser(packet_out pkt,
                              inout header_t hdr,
                              in metadata_t eg_md,
                              in egress_intrinsic_metadata_for_deparser_t 
                                eg_intr_dprsr_md
                              ) {

    apply {
        pkt.emit(hdr);
    }
}

Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;
