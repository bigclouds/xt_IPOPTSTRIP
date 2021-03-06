#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/skbuff.h>
#include <net/checksum.h>
#include <net/ip.h>
#include "xt_IPOPTSTRIP.h"

#define IPV4_HL 5
#define IPV4_LEN 20

#ifdef DEBUG
static void print_skb_header_offsets(struct sk_buff *skb) {
	printk("Transport header offset: %u\n", skb->transport_header);
	printk("Network header offset: %u\n", skb->network_header);
	printk("MAC header offset: %u\n", skb->mac_header);
}
#endif

static unsigned int ipoptstrip_tg(struct sk_buff *skb,
	const struct xt_action_param *par) {
		
	struct ip_options *opt = &(IPCB(skb)->opt);
	unsigned char *opt_ptr, *opt_end_ptr;
	struct iphdr *iphdr;
	const struct xt_ipoptstrip_tg_info *info;
	__wsum csum32;
	
	if (opt->optlen > 0) {
		iphdr = ip_hdr(skb);
		info = par->targinfo;
		
#ifdef DEBUG
		printk("flags: %x\n", info->flags);
		printk("Packet with IP options (%i bytes) from: %pI4 to: %pI4\n", 
			opt->optlen, &iphdr->saddr, &iphdr->daddr);
		print_skb_header_offsets(skb);
#endif
		if (! XT_IPOPTSTRIP_IS_SET(info->flags, XT_IPOPTSTRIP_KEEP_DST)) {
			opt_ptr = (unsigned char*) &iphdr[1];
			opt_end_ptr = opt_ptr + opt->optlen;
			
			for (; opt_ptr < opt_end_ptr; opt_ptr++) {
				
				switch (*opt_ptr) {
					case IPOPT_LSRR:
					case IPOPT_SSRR:
						/* Re-write destination field with last address */
						memcpy(&iphdr->daddr, (opt_ptr+(opt_ptr[1]))-4, 4);
						break;
				}
			}
		}
		
		/* Alter header and total lengths */
		iphdr->ihl = IPV4_HL; // 5 32-bit words in IPv4 header with no options
		iphdr->tot_len -= cpu_to_be16(opt->optlen);
		
		/* Move transport header pointer to after network header */
		skb_set_transport_header(skb, IPV4_LEN);
		
		/* Move remaining data up the buffer */
		memmove(skb_transport_header(skb), skb_transport_header(skb) + opt->optlen,
			skb->tail - (skb->transport_header + opt->optlen));
			
		/* Remove un-needed buffer space */
		skb_trim(skb, (skb->len - opt->optlen));
		
		/* Re-calculate IP header checksum */
		csum32 = csum_partial(iphdr, sizeof(struct iphdr), 0);
		iphdr->check = csum_fold(csum32);
		
#ifdef DEBUG
		print_skb_header_offsets(skb);
#endif
	}
	
	return XT_CONTINUE;
}

static struct xt_target ipoptstrip_tg_reg __read_mostly = {
	.name		= "IPOPTSTRIP",
	.revision	= 0,
	.family		= NFPROTO_IPV4,
	.target		= ipoptstrip_tg,
	.targetsize	= sizeof(struct xt_ipoptstrip_tg_info),
	.me			= THIS_MODULE,
};

static int __init ipoptstrip_tg_init(void) {	
	return xt_register_target(&ipoptstrip_tg_reg);
}

static void __exit ipoptstrip_tg_exit(void) {
	return xt_unregister_target(&ipoptstrip_tg_reg);
}

MODULE_DESCRIPTION("Strip/re-insert IP Options");
MODULE_AUTHOR("Duncan Eastoe");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xt_IPOPTSTRIP");
module_init(ipoptstrip_tg_init);
module_exit(ipoptstrip_tg_exit);
