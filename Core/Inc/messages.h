/* messages.h */

#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char *topic;
    uint8_t *payload;
    uint32_t length;
} message_t;

void messages_init(void);
void messages_destroy(message_t *message);
bool messages_send(message_t *message);

#endif /* MESSAGES_H */
