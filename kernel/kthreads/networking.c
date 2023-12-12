/**
 * @file networking.c
 * @author Joe Bayer (joexbayer)
 * @brief Main process for handling all networking traffic. 
 * @version 0.1
 * @date 2022-06-04
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <kutils.h>
#include <scheduler.h>
#include <serial.h>
#include <assert.h>
#include <kthreads.h>
#include <work.h>

#include <net/interface.h>
#include <net/netdev.h>
#include <net/net.h>
#include <net/packet.h>
#include <net/skb.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/tcp.h>
#include <net/icmp.h>
#include <net/socket.h>
#include <net/net.h>
#include <net/dhcp.h>

#define MAX_PACKET_SIZE 0x600

enum NETD_STATES {
    NETD_UNINITIALIZED,
    NETD_STARTED
};

static struct networkmanager;
static struct network_manager_ops {
    void (*start)();
    void (*stop)();
    void (*restart)();

    void (*get_info)(struct net_info* info);
    void (*send_skb)(struct networkmanager* nm, struct sk_buff* skb);
};

static struct networkmanager {
    int state;

    uint16_t packets;
    struct skb_queue* skb_tx_queue;
    struct skb_queue* skb_rx_queue;

    struct net_info stats;

    struct network_manager_ops ops;

    struct net_interface* ifs[4];
    uint8_t if_count; 

} netd = {
    .ops = {
        .start = NULL,
        .stop = NULL,
        .restart = NULL
    },
    .state = NETD_UNINITIALIZED,
    .packets = 0,
    .stats.dropped = 0, 
    .stats.recvd = 0,
    .stats.sent = 0
};

static struct net_interface* __net_find_interface(char* dev)
{
    int len = strlen(dev);
    for (int i = 0; i < netd.if_count; i++){
        if(strncmp(netd.ifs[i]->name, dev, strlen(len)) == 0) return netd.ifs[i];
    }
    return NULL;
}

static struct net_interface* __net_interface(struct netdev* dev)
{
    for (int i = 0; i < netd.if_count; i++){
        if(netd.ifs[i]->device == dev) return netd.ifs[i];
    }
    return NULL;
}

int net_configure_iface(char* dev, uint32_t ip, uint32_t netmask, uint32_t gateway)
{
    struct net_interface* interface = __net_find_interface("eth0");
    if(interface == NULL) return -1;

    interface->ip = ip;
    interface->netmask = 0xffffff00;
    interface->gateway = gateway;
    interface->ops->configure(interface, "eth0");

    return 0;

}

void __callback net_incoming_packet(struct netdev* dev)
{
    if(dev == NULL) return;

    struct net_interface* interface = __net_interface(dev);
    if(interface == NULL) return;

    struct sk_buff* skb = skb_new();
    skb->len = dev->read(skb->data, MAX_PACKET_SIZE);
    if(skb->len <= 0) {
        dbgprintf("Received an empty packet.\n");
        skb_free(skb);
        return;
    }
    skb->interface = interface;

    netd.skb_rx_queue->ops->add(netd.skb_rx_queue, skb);
    netd.packets++;
    netd.stats.recvd++;
    dbgprintf("New packet incoming...\n");
}

int net_list_ifaces()
{
    for (int i = 0; i < netd.if_count; i++){
        twritef("%s: %s mtu 1500\n", netd.ifs[i]->name, netd.ifs[i]->state == NET_IFACE_UP ? "UP" : "DOWN");
        twritef("   inet %i netmask %i\n", ntohl(netd.ifs[i]->ip), ntohl(netd.ifs[i]->netmask));
        twritef("   tx %d   rx %d\n", netd.ifs[i]->device->sent, netd.ifs[i]->device->received);
    }
    
    return netd.if_count;
}

int net_register_interface(struct net_interface* interface)
{
    if(netd.if_count >= 4) return -1;
    netd.ifs[netd.if_count++] = interface;
    return 0;
}

void __deprecated net_incoming_packet_handler()
{
    dbgprintf("New packet incoming...\n");

    struct sk_buff* skb = skb_new();
    skb->len = netdev_recieve(skb->data, MAX_PACKET_SIZE);
    if(skb->len <= 0) {
        dbgprintf("Received an empty packet.\n");
        skb_free(skb);
        return;
    }

    netd.skb_rx_queue->ops->add(netd.skb_rx_queue, skb);
    netd.packets++;
    netd.stats.recvd++;
    dbgprintf("New packet incoming...\n");
}

int net_send_skb(struct sk_buff* skb)
{
    ERR_ON_NULL(netd.skb_tx_queue);

    RETURN_ON_ERR(netd.skb_tx_queue->ops->add(netd.skb_tx_queue, skb));
    netd.packets++;
    dbgprintf("Added SKB to TX queue\n");

    return 0;
    
}

static void __net_config_loopback()
{
    struct net_interface* interface = __net_find_interface("lo0");
    if(interface == NULL) return;

    interface->ip = 0x7f000001;
    interface->netmask = 0xff000000;
    interface->gateway = 0x7f000001;
}

static void __net_transmit_skb(struct sk_buff* skb)
{
    dbgprintf("Transmitting packet\n");
    twritef("-> %i:%d, %d\n", ntohl(skb->hdr.ip->daddr), skb->hdr.tcp->dest == 0 ? ntohs(skb->hdr.udp->destport) : ntohs(skb->hdr.tcp->dest) , skb->len);
    int read = netdev_transmit(skb->head, skb->len);
    if(read <= 0){
        dbgprintf("Error sending packet\n");
    }
    netd.packets++;
    netd.stats.sent++;
}

int net_drop_packet(struct sk_buff* skb)
{
    current_netdev.dropped++;
    netd.stats.dropped++;
    skb_free(skb);
    return 0;
}

error_t net_get_info(struct net_info* info)
{
    *info = netd.stats;
    return ERROR_OK;
}

int net_handle_recieve(struct sk_buff* skb)
{
    dbgprintf("Parsing new packet\n");
    if(net_ethernet_parse(skb) < 0) return net_drop_packet(skb);
    switch(skb->hdr.eth->ethertype){
        /* Ethernet type is IP */
        case IP:
            if(net_ipv4_parse(skb) < 0) return net_drop_packet(skb);
            switch (skb->hdr.ip->proto){
            case UDP:
                if(net_udp_parse(skb) < 0) return net_drop_packet(skb);
                twritef("<- %i:%d, %d\n", ntohl(skb->hdr.ip->daddr), skb->hdr.udp->destport, skb->len);
                break;
            
            case TCP:
                if(tcp_parse(skb) < 0) return net_drop_packet(skb);
                twritef("<- %i:%d, %d\n", ntohl(skb->hdr.ip->daddr), skb->hdr.tcp->dest, skb->len);
                skb_free(skb);
                break;
            
            case ICMPV4:
                if(net_icmp_parse(skb) < 0) return net_drop_packet(skb);
                net_icmp_handle(skb);
                skb_free(skb);
                break;
            default:
                return net_drop_packet(skb);
            }
            break;

        /* Ethernet type is ARP */
        case ARP:
            if(arp_parse(skb) < 0) return net_drop_packet(skb);
            // send arp response.
            dbgprintf("Recieved ARP packet.\n");
            skb_free(skb);
            break;

        default:
            return net_drop_packet(skb);
    }
    return 1;
}

/**
 * @brief Main networking event loop.
 * 
 */
void __kthread_entry networking_main()
{
    if(!is_netdev_attached()) return;

    __net_config_loopback();

    if(netd.state == NETD_UNINITIALIZED){
        netd.skb_rx_queue = skb_new_queue();
        netd.skb_tx_queue = skb_new_queue();
    }

    start("dhcpd", 0, NULL);

    while(1){
        /**
         * @brief Query RX an    TX queue for netd.packets.
         */
        if(SKB_QUEUE_READY(netd.skb_tx_queue)){
            dbgprintf("Sending new SKB from TX queue\n");
            struct sk_buff* skb = netd.skb_tx_queue->ops->remove(netd.skb_tx_queue);
            assert(skb != NULL);
            __net_transmit_skb(skb);
            skb_free(skb);
        }

        if(SKB_QUEUE_READY(netd.skb_rx_queue)){
            dbgprintf("Receiving new SKB from RX queue\n");
            struct sk_buff* skb = netd.skb_rx_queue->ops->remove(netd.skb_rx_queue);
            assert(skb != NULL);

            /* Offload skb parsing to worker thread. */
            //work_queue_add(&net_handle_recieve, (void*)skb, NULL);
            net_handle_recieve(skb);
        }

        kernel_yield();
    }
}