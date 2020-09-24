/* config.h */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

bool config_get_eui64(uint8_t *eui);
bool config_get_eui48(uint8_t *eui);

#endif /* CONFIG_H */
