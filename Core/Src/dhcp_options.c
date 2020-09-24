/* dhcp_options.c */

/* Extra DHCP functionality */

#include <string.h>

#include "lwip.h"
#include "dhcp.h"
#include "prot/dhcp.h"
#include "dhcp_options.h"

char dhcp_domain_name[DHCP_DOMAIN_NAME_LEN+1] = "local";

void dhcp_append_options_hook(struct netif *netif, struct dhcp *dhcp,
        u8_t state, struct dhcp_msg *msg, u8_t msg_type,
        u16_t *options_len_ptr)
{
    int i;

    LWIP_ASSERT("dhcp option overflow", *options_len_ptr + 1 <= DHCP_OPTIONS_LEN);

    for (i = 0; i < *options_len_ptr; i += msg->options[i+1]+2)
        if (msg->options[i] == 55)
            break;
 
    int optlen = msg->options[i+1];
    u8_t *p = msg->options + i + optlen + 2;
    memmove(p+1, p, *options_len_ptr - i - optlen - 2);

    msg->options[i+1]++;
    msg->options[i+optlen+2] = 15; /* Domain name */

    *options_len_ptr += 1;
}

void dhcp_parse_options_hook(struct netif *netif, struct dhcp *dhcp,
        u8_t state, struct dhcp_msg *msg, u8_t msg_type,
        u8_t option, u8_t option_len, struct pbuf *pbuf,
        u16_t option_value_offset)
{
    printf("Parse options hook: %d\n", option);
    if (option == 15) { /* Domain name */
        int len = LWIP_MIN(option_len, DHCP_DOMAIN_NAME_LEN);
        u8_t *ptr = (u8_t *)pbuf_get_contiguous(pbuf, (u8_t *)dhcp_domain_name, DHCP_DOMAIN_NAME_LEN, len, option_value_offset); 
        if (!ptr)
            return;
        if (ptr != (u8_t *)dhcp_domain_name)
            memcpy(dhcp_domain_name, ptr, len);
        dhcp_domain_name[len] = '\0';
        printf("Got domain name: %s\n", dhcp_domain_name);
    }
}

