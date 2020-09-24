/* messages.c */

#include "messages.h"
#include "lwip.h"
#include "queue.h"
#include "mqtt_manager.h"
#include "jsmn.h"
#include "radio.h"
#include "radio_reg.h"

#include <stdbool.h>
#include <string.h>

osThreadId_t messages_task_handle;
const osThreadAttr_t messages_task_attributes = {
  .name = "messages",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4 * 2
};

#define QUEUE_LENGTH 10

static StaticQueue_t messages_queue;
static QueueHandle_t queue_handle;
uint8_t messages_queue_storage[QUEUE_LENGTH * sizeof(message_t)];

void messages_task(void *argument);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

void messages_init(void)
{
    messages_task_handle = osThreadNew(messages_task, NULL, &messages_task_attributes);
}

void messages_destroy(message_t *message)
{
    if (message->topic) {
        free(message->topic);
        message->topic = NULL;
    }
    if (message->payload) {
        free(message->payload);
        message->payload = NULL;
    }
    message->length = 0;
}

typedef struct {
    int ntokens;
    jsmntok_t tokens[];
} json_message;

json_message *message_parse_json(uint8_t *payload, uint32_t length)
{
    jsmn_parser parser;
    json_message *message;

    jsmn_init(&parser);

    int ntokens = jsmn_parse(&parser, (char *)payload, length, NULL, 0);

    printf("Tokens required: %d\n", ntokens);

    if (ntokens <= 0)
        return NULL;

    message = malloc(sizeof(json_message) + sizeof(jsmntok_t) * ntokens);
    if (!message)
        return NULL;

    message->ntokens = ntokens;

    jsmn_init(&parser);
    int result = jsmn_parse(&parser, (char *)payload, length, message->tokens, ntokens);

    printf("Tokens parsed: %d\n", result);

    if (result < 0) {
        free(message);
        return NULL;
    }

    message->ntokens = result;

    return message;
}

bool message_parse_bool(uint8_t *payload, uint32_t length)
{
    if (length == 0)
        return false;
    if (length == 1)
        return *payload == '1';
    if (strncmp((char *)payload, "on", length) == 0)
        return true;
    if (strncmp((char *)payload, "ON", length) == 0)
        return true;
    return false;
}

message_t *messages_deepcopy(message_t *message)
{
    message_t *copy = malloc(sizeof(message_t));
    if (!copy)
        return NULL;

    copy->topic = NULL;
    copy->payload = NULL;
    copy->length = 0;

    if (message->topic) {
        copy->topic = malloc(strlen(message->topic)+1);
        if (!copy->topic) {
            free(copy);
            return NULL;
        }
        strcpy(copy->topic, message->topic);
    }

    if (message->payload) {
        copy->payload = malloc(message->length);
        if (!copy->payload) {
            messages_destroy(copy);
            free(copy);
            return NULL;
        }
        memcpy(copy->payload, message->payload, message->length);
    }
    copy->length = message->length;

    return copy;
}

void messages_publish(message_t *message)
{
    message_t *copy = messages_deepcopy(message);

    printf("Publishing message\n");
    if (!copy)
        return;

    printf("Passing on to mqtt manager\n");
    mqtt_manager_send(copy);
}

void messages_publish_simple(char *topic, char *payload)
{
    message_t message;
    message.topic = topic;
    message.payload = (uint8_t *)payload;
    message.length = strlen(payload);

    messages_publish(&message);
}

void message_heater_get(uint8_t *payload, uint32_t length)
{
    printf("Get heater state\n");

    messages_publish_simple("status/heater", get_heater()?"1":"0");
}

void message_heater_set(uint8_t *payload, uint32_t length)
{
    bool state = message_parse_bool(payload, length);
    printf("Set heater state: %s\n", state?"on":"off");
    set_heater(state);
}

static struct { const char *name; uint8_t file; uint16_t reg; uint8_t len; } rset[] = {
#define X(name)    { #name, RREG(name), RLEN(name) },
    X(DEV_ID)
    X(SYS_STATUS)
//    X(SYS_STATE)
    X(SYS_CFG)
//    X(SYS_MASK)
    X(SYS_CTRL)
//    X(EC_CTRL)
//    X(OTP_SF)
//    X(OTP_CTRL)
//    X(FS_PLLCFG)
//    X(FS_PLLTUNE)
//    X(FS_XTALT)
//    X(RF_RXCTRLH)
//    X(RF_TXCTRL)
//    X(DRX_TUNE0B)
//    X(DRX_TUNE1A)
//    X(DRX_TUNE1B)
//    X(DRX_TUNE4H)
//    X(DRX_TUNE2)
//    X(DRX_SFDTOC)
//    X(AGC_TUNE2)
//    X(AGC_TUNE1)
//    X(USR_SFD)
//    X(CHAN_CTRL)
//    X(TX_FCTRL)
//    X(RX_FWTO)
//    X(RX_FINFO)
//    X(LDE_CFG1)
//    X(LDE_CFG2)
//    X(LDE_REPC)
//    X(DX_TIME)
//    X(TX_TIME)
//    X(RX_TIME)
    X(PMSC_CTRL0)
    X(PMSC_CTRL1)
    X(PMSC_LEDC)
    X(GPIO_MODE)
//    X(AON_WCFG)
//    X(AON_CFG0)
//    X(AON_CFG1)
//    X(AON_CTRL)
//    X(LDE_RXANTD)
//    X(TX_ANTD)
//    X(TX_POWER)
#undef X
};

void message_radio_debug_get(uint8_t *payload, uint32_t length)
{
    int r, i;
    char *output;
    uint8_t *buf;
    char *op;
    int totlen = 0;
    int maxlen = 0;

    for (r = 0; r < ARRAY_SIZE(rset); r++) {
        int len = rset[r].len;
        if (len > maxlen)
            maxlen = len;
        /* name : space x x space x x \n */
        totlen += strlen(rset[r].name) + 2 + 3*len;
    }
    
    output = malloc(totlen+1);
    if (!output)
        return;

    buf = malloc(maxlen);
    if (!buf) {
        free(output);
        return;
    }

    op = output;

    for (r = 0; r < ARRAY_SIZE(rset); r++) {
        uint8_t file = rset[r].file;
        uint16_t reg = rset[r].reg;
        int len = rset[r].len;
        
        radio_read(file, reg, (uint8_t *)buf, len);

        op += sprintf(op, "%s:", rset[r].name);
        printf("%s:", rset[r].name);

        for (i = 0; i < len; i++) {
            op += sprintf(op, " %02x", buf[i]);
            printf(" %02x", buf[i]);
        }

        op += sprintf(op, "\n");
        printf("\n");
    }
    free(buf);
    printf("length is %d, %d\n", op - output, strlen(output));
    printf("%s\n", output);
    messages_publish_simple("state/radio_debug", output);
    free(output);
}

const char *message_token_type(int i)
{
    switch (i) {
    case JSMN_UNDEFINED:
        return "undefined";
    case JSMN_OBJECT:
        return "object";
    case JSMN_ARRAY:
        return "array";
    case JSMN_STRING:
        return "string";
    case JSMN_PRIMITIVE:
        return "primitive";
    default:
        return "(invalid)";
    }
}

bool message_token_int(jsmntok_t *token, uint8_t *payload, uint32_t length, int *value)
{
    if (token->type != JSMN_PRIMITIVE)
        return false;

    if (!isdigit(payload[token->start]))
        return false;

    *value = atoi((char *)payload + token->start);
    return true;
}

typedef struct {
    const char *string;
    int value;
} string_list;

int message_token_string_select(jsmntok_t *token, uint8_t *payload, uint32_t length, string_list *strings)
{
    if (token->type != JSMN_STRING)
        return 0;
    while (strings->string) {
        if (strncmp((char *)payload + token->start, strings->string, token->end - token->start) == 0)
            return strings->value;
        strings++;
    }
    return strings->value;
}

#define RADIO_GPIO_START  0
#define RADIO_GPIO_PIN    1
#define RADIO_GPIO_OBJECT 2
#define RADIO_GPIO_VALUE  3
#define RADIO_GPIO_ABORT  4

void message_radio_gpio_set(uint8_t *payload, uint32_t length)
{
    json_message *message = message_parse_json(payload, length);

    if (!message)
        return;
#if 0
    for (int i = 0; i < message->ntokens; i++) {
        printf("Token type %s contents '%.*s', size %d\n",
            message_token_type(message->tokens[i].type),
            message->tokens[i].end-message->tokens[i].start,
            payload+message->tokens[i].start,
            message->tokens[i].size);
    }
#endif

    int pin;
    int mode;
    int dir;
    int value;
    int selector;
    int remaining;

    int state = RADIO_GPIO_START;

    for (int i = 0; i < message->ntokens; i++) {
        printf("Token type %s contents '%.*s', size %d, state %d\n",
            message_token_type(message->tokens[i].type),
            message->tokens[i].end-message->tokens[i].start,
            payload+message->tokens[i].start,
            message->tokens[i].size, state);
        switch (state) {
        case RADIO_GPIO_START:
            if (message_token_int(&(message->tokens[i]), payload, length, &pin)) {
                if (message->tokens[i].size == 1) {
                    state = RADIO_GPIO_PIN;
                    mode = 0;
                    dir = 0;
                    value = 0;
                    break;
                }
            }
            state = RADIO_GPIO_ABORT;
            break;
        case RADIO_GPIO_PIN:
            if (message->tokens[i].type == JSMN_OBJECT) {
                remaining = message->tokens[i].size;
                if (remaining)
                    state = RADIO_GPIO_OBJECT;
                else
                    state = RADIO_GPIO_START;
            } else {
                state = RADIO_GPIO_ABORT;
            }
            break;
        case RADIO_GPIO_OBJECT:
            {
                string_list list[] = {{"mode", 1}, {"dir", 2}, {"value", 3}, {NULL, 0}};
                selector = message_token_string_select(&(message->tokens[i]), payload, length, list);
            }
            switch (message->tokens[i].size) {
            case 0:
                remaining--;
                if (!remaining) {
                    state = RADIO_GPIO_START;
                    printf("radio_gpio: pin %d, mode %d, dir %d, value %d\n", pin, mode, dir, value);
                    radio_gpio(pin, mode, dir, value);
                }
                break;
            case 1:
                state = RADIO_GPIO_VALUE;
                break;
            default:
                state = RADIO_GPIO_ABORT;
                break;
            }

            break;
        case RADIO_GPIO_VALUE:
            if (message->tokens[i].type != JSMN_PRIMITIVE) {
                state = RADIO_GPIO_ABORT;
                return;
            }
            int n;
            if (message_token_int(&(message->tokens[i]), payload, length, &n)) {
                int *ptrs[] = {NULL, &mode, &dir, &value};
                int *ptr = ptrs[selector];
                if (ptr)
                    *ptr = n;
            }
            state = RADIO_GPIO_OBJECT;
            remaining--;
            if (!remaining) {
                state = RADIO_GPIO_START;
                printf("radio_gpio: pin %d, mode %d, dir %d, value %d\n", pin, mode, dir, value);
                radio_gpio(pin, mode, dir, value);
            }
            break;
        case RADIO_GPIO_ABORT:
        default:
            free(message);
            return;
        }
    }

    free(message);
}

static struct {
    const char *topic;
    void (*function)(uint8_t *payload, uint32_t length);
} topic_dispatch[] = {
    {"get/heater", message_heater_get},
    {"set/heater", message_heater_set},
    {"set/radio_gpio", message_radio_gpio_set},
    {"get/radio_debug", message_radio_debug_get}
};

void messages_dispatch(message_t *message)
{
    printf("Messages task received message:\n");
    printf("  Topic: %s\n", message->topic);
    printf("  Payload: %.*s\n", (int)message->length, message->payload);
    for (int i = 0; i < ARRAY_SIZE(topic_dispatch); i++) {
        if (strcmp(message->topic, topic_dispatch[i].topic) == 0) {
            (topic_dispatch[i].function)(message->payload, message->length);
            return;
        }
    }
}

void messages_task(void *argument)
{
    queue_handle = xQueueCreateStatic(QUEUE_LENGTH, sizeof(message_t),
                                      messages_queue_storage, &messages_queue);

    configASSERT(queue_handle);

    for (;;) {
        message_t message;
        if (xQueueReceive(queue_handle, &message, portMAX_DELAY)) {
            messages_dispatch(&message);
            messages_destroy(&message);
        }
    }
}

bool messages_send(message_t *message)
{
    if (xQueueSend(queue_handle, message, 0) != pdTRUE)
        return false;
    return true;
}

