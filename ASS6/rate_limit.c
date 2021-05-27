#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

static int threshold = 1 << 20;
MODULE_PARM_DESC(threshold, "Threshhold before rate limit. The default value is 1MB");
module_param(threshold, int, 0644);

static int rate = 2000;
MODULE_PARM_DESC(rate, "Limited rate. The default rate is 2000bytes/ms");
module_param(rate, int, 0644);

struct myflow {
    unsigned int saddr;
    unsigned int daddr;
    unsigned short int sport;
    unsigned short int dport;

    // State
    unsigned int pkt_count, drop_count, byte_count, byte_drop_count;
    unsigned long time;
    unsigned long byte;
    unsigned long start;
};

#define FLOW_TABLE_SIZE 4096
static struct myflow flow_table[FLOW_TABLE_SIZE];

/* Hash function */
static inline unsigned int hash(unsigned int saddr, unsigned daddr, unsigned short int sport, unsigned short int dport)
{
    return ((saddr % FLOW_TABLE_SIZE + 1) * (daddr % FLOW_TABLE_SIZE + 1) * (sport % FLOW_TABLE_SIZE + 1) * (dport % FLOW_TABLE_SIZE + 1)) % FLOW_TABLE_SIZE;
}

static inline void reset_flow(struct myflow* flow)
{
    memset(flow, 0, sizeof(struct myflow));
    flow->time = jiffies;
    flow->start = 0;
}

static struct myflow* get_flow(unsigned int saddr, unsigned daddr, unsigned short int sport, unsigned short int dport)
{
    struct myflow* flow;
    flow = &flow_table[hash(saddr, daddr, sport, dport)];

    if (flow->saddr == 0 && flow->daddr == 0 && flow->sport == 0 && flow->dport == 0) {
        flow->saddr = saddr;
        flow->daddr = daddr;
        flow->sport = sport;
        flow->dport = dport;
    }

    return flow;
}

/* Hook function */
static unsigned int rate_limit(void* priv, struct sk_buff* skb, const struct nf_hook_state* state)
{
    struct iphdr* iph;
    struct tcphdr* tcph;
    unsigned int saddr, daddr;
    unsigned short int sport, dport;
    struct myflow* flow;
    unsigned int payload_len;
    int ret;

    iph = ip_hdr(skb);
    if (iph->protocol != IPPROTO_TCP)
        return NF_ACCEPT;

    tcph = tcp_hdr(skb);
    saddr = iph->saddr;
    daddr = iph->daddr;
    sport = ntohs(tcph->source);
    dport = ntohs(tcph->dest);

    flow = get_flow(saddr, daddr, sport, dport);
    payload_len = ntohs(iph->tot_len) - (iph->ihl << 2) - (tcph->doff << 2);

    if (payload_len == 0) {
        ret = NF_ACCEPT;
        goto out;
    }

    if (flow->start == 0) {
        flow->start = jiffies;
    }

    if (flow->byte_count < threshold) {
        flow->pkt_count++;
        flow->byte_count += payload_len;
        flow->time = jiffies;
        flow->byte += payload_len;
        ret = NF_ACCEPT;
    }
    else {
        long delta = (jiffies - flow->time) * 1000 / HZ;
        if (flow->byte_count - flow->byte > delta * rate) {
            flow->drop_count++;
            flow->byte_drop_count += payload_len;
            ret = NF_DROP;
        }
        else {
            flow->pkt_count++;
            flow->byte_count += payload_len;
            ret = NF_ACCEPT;
        }
    }
out:
    if (unlikely(tcph->fin)) {
        printk(KERN_INFO "[Finish rate = %d]  t = %ld accept/drop (bytes) :  %d/%d", rate, (jiffies - flow->start) * 1000/HZ, flow->byte_count, flow->byte_drop_count);
        reset_flow(flow);
    }
    return ret;
}

static const struct nf_hook_ops rate_limit_nf_ops = {
    .hook = rate_limit,
    .pf = NFPROTO_INET,
    .hooknum = NF_INET_PRE_ROUTING,
    .priority = NF_IP_PRI_FIRST,
};

static int __init rate_limit_init_module(void)
{
    int err;
    int i;

    for (i = 0; i < FLOW_TABLE_SIZE; ++i)
        reset_flow(&flow_table[i]);

    err = nf_register_net_hook(&init_net, &rate_limit_nf_ops);
    if (err < 0)
        return err;

    return 0;
}

static void __exit rate_limit_exit_module(void)
{
    nf_unregister_net_hook(&init_net, &rate_limit_nf_ops);
}

module_init(rate_limit_init_module);
module_exit(rate_limit_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author's name");
MODULE_DESCRIPTION("Netfilter module to drop packets with flow");