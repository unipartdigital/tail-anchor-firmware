/* dhcp_options.h */

#ifndef DHCP_OPTIONS_H
#define DHCP_OPTIONS_H

#define DHCP_DOMAIN_NAME_LEN 32
extern char dhcp_domain_name[DHCP_DOMAIN_NAME_LEN+1];

#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"

void dhcp_append_options_hook(struct netif *netif, struct dhcp *dhcp,
    u8_t state, struct dhcp_msg *msg, u8_t msg_type,
    u16_t *options_len_ptr);

void dhcp_parse_options_hook(struct netif *netif, struct dhcp *dhcp,
    u8_t state, struct dhcp_msg *msg, u8_t msg_type,
    u8_t option, u8_t option_len, struct pbuf *pbuf,
    u16_t option_value_offset);

#endif /* DHCP_OPTIONS_H */
