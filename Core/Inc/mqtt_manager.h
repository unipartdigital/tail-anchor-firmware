/* mqtt_manager.h */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "lwip.h"
#include "messages.h"

void mqtt_manager_start(void);
void network_status_changed(struct netif *netif);
void mqtt_manager_send(message_t *message);

#endif /* MQTT_MANAGER_H */
