/* mqtt_manager.c */

#include <string.h>

#include "lwip.h"
#include "mqtt.h"
#include "lwip/dns.h"
#include "mqtt_manager.h"
#include "dhcp_options.h"
#include "messages.h"
#include "config.h"

/* for mqtt_client_t storage size, ugh */
#include "mqtt_priv.h"

static mqtt_client_t client;

#define MQTT_STATE_NOT_CONNECTED 0
#define MQTT_STATE_CONNECTING    1
#define MQTT_STATE_CONNECTED     2
#define MQTT_STATE_RETRY         3

//static int mqtt_state = MQTT_STATE_NOT_CONNECTED;

static char *eui64_compact_string(void)
{
    static char s[17];

    uint8_t eui64[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    (void) config_get_eui64(eui64);

    sprintf(s, "%02x%02x%02x%02x%02x%02x%02x%02x",
        eui64[0], eui64[1], eui64[2], eui64[3],
        eui64[4], eui64[5], eui64[6], eui64[7]);

    return s;
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_sub_request_cb(void *arg, err_t result);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

void mqtt_manager_connect(void *arg);

void mqtt_do_connect(mqtt_client_t *client, const ip_addr_t *addr)
{
    struct mqtt_connect_client_info_t ci;
    err_t err;
    char id[24];

    sprintf(id, "anchor_%s", eui64_compact_string());

    memset(&ci, 0, sizeof(ci));
    ci.client_id = id;

    err = mqtt_client_connect(client, addr, MQTT_PORT, mqtt_connection_cb, 0, &ci);

    if (err != ERR_OK) {
        printf("Failed to connect to MQTT\n");
//        mqtt_manager_set_retry();
    }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
    err_t err;
    char topic[26];

    sprintf(topic, "anchor/%s/#", eui64_compact_string());

    if (status == MQTT_CONNECT_ACCEPTED) {
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, arg);

        err = mqtt_subscribe(client, topic, 0, mqtt_sub_request_cb, arg);

        if (err != ERR_OK) {
            printf("mqtt: failed to subscribe: %d\n", err);
        }
    } else {
        printf("mqtt: disconnected: %d\n", status);
        mqtt_manager_connect(NULL);
    }
}

static void mqtt_sub_request_cb(void *arg, err_t result)
{
    printf("Subscribe result: %d\n", result);
}


static struct {
    message_t message;
    uint32_t offset;
} inpub_info = {{NULL, NULL, 0}, 0};

static void inpub_free(void)
{
    messages_destroy(&inpub_info.message);
    inpub_info.offset = 0;
}

static void inpub_reset(void)
{
    inpub_info.message.topic = NULL;
    inpub_info.message.payload = NULL;
    inpub_info.message.length = 0;
    inpub_info.offset = 0;
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
    char prefix[24];

    sprintf(prefix, "anchor/%s", eui64_compact_string());

    printf("Incoming publish at topic %s, length %u\n", topic, (unsigned int) tot_len);

    if (inpub_info.message.topic) {
        printf("Still processing previous topic %s\n", inpub_info.message.topic);
        inpub_free();
    }

    if (strncmp(prefix, topic, strlen(prefix))) {
        printf("Not for us\n");
        return;
    }

    topic += strlen(prefix);
    while (*topic == '/')
        topic++;

    printf("Message is for us, on our topic %s\n", topic);

    inpub_info.message.topic = malloc(strlen(topic)+1);
    if (!inpub_info.message.topic) {
        printf("malloc() failed\n");
        return;
    }

    if (tot_len) {
        inpub_info.message.payload = malloc(tot_len);
        if (!inpub_info.message.payload) {
            printf("malloc() failed\n");
            inpub_free();
            return;
        }
    } else {
        inpub_info.message.payload = NULL;
    }

    strcpy(inpub_info.message.topic, topic);

    inpub_info.message.length = tot_len;
    inpub_info.offset = 0;
}

static void mqtt_forward_message(void)
{
    if (messages_send(&inpub_info.message)) {
        inpub_reset();
    } else {
        printf("Failed to send message\n");
        inpub_free();
    }
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    printf("Incoming publish payload with length %d, flags %u\n", len, (unsigned int) flags);

    if (inpub_info.offset + len > inpub_info.message.length) {
        printf("Buffer overflow detected\n");
        len = inpub_info.message.length - inpub_info.offset;
    }

    memcpy(inpub_info.message.payload + inpub_info.offset, data, len);
    inpub_info.offset += len;

    if (flags & MQTT_DATA_FLAG_LAST) {
        if (inpub_info.offset < inpub_info.message.length)
            printf("Payload is incomplete\n");
        /* Process payload */
        printf("Payload is %.*s\n", (int)inpub_info.message.length, inpub_info.message.payload);
        mqtt_forward_message();
    }
}


#define MQTT_HOSTNAME "mqtt"
#define MQTT_HOSTNAME_LEN 4

void mqtt_manager_dns_found(const char *name, const ip_addr_t *addr, void *arg)
{
    if (!addr) {
        printf("DNS resolution failed\n");
        return;
    }

    mqtt_do_connect(&client, addr);
}

void mqtt_manager_do_send(void *arg)
{
    message_t *message = arg;
    err_t err;
    char prefix[24];

    sprintf(prefix, "anchor/%s", eui64_compact_string());

    printf("mqtt_manager_do_send\n");

    char *topic = malloc(strlen(message->topic) + 25);
    if (!topic) {
        messages_destroy(message);
        free(message);
        return;
    }
    sprintf(topic, "anchor/%s/%s", eui64_compact_string(), message->topic);

    err = mqtt_publish(&client, topic, message->payload, message->length, 0, 0, NULL, NULL);
    if (err != ERR_OK) {
        printf("Failed to publish: %d\n", err);
    }
    free(topic);
    messages_destroy(message);
    free(message);
}

void mqtt_manager_send(message_t *message)
{
    err_t err;

    printf("mqtt_manager_send\n");
    err = tcpip_callback(mqtt_manager_do_send, message);

    if (err != ERR_OK) {
        printf("Unable to call mqtt_manager_do_send\n");
        messages_destroy(message);
        free(message);
    }
}


void mqtt_manager_connect(void *arg)
{
    err_t err;
    ip_addr_t addr;
    char hostname[DHCP_DOMAIN_NAME_LEN + MQTT_HOSTNAME_LEN + 2];

    sprintf(hostname, "%s.%s", MQTT_HOSTNAME, dhcp_domain_name);

    printf("Calling dns_gethostbyname\n");
    err = dns_gethostbyname(hostname, &addr, mqtt_manager_dns_found, NULL);

    if (err == ERR_OK) {
        mqtt_do_connect(&client, &addr);
    } else if (err != ERR_INPROGRESS) {
        printf("DNS resolution error\n");
    }
}

void mqtt_manager_start(void)
{
    err_t err;

    printf("Arranging callback\n");
    err = tcpip_callback(mqtt_manager_connect, NULL);

    if (err != ERR_OK) {
        printf("Unable to call mqtt_manager_connect\n");
    }
}


void network_status_changed(struct netif *netif)
{
    printf("Network status changed\n");
    if (!netif_is_up(netif))
        return;

    printf("Starting DNS resolution\n");
    mqtt_manager_start();
}
