/* config.c */

#include "config.h"

bool config_get_eui64(uint8_t *eui)
{
    eui[0] = 0x00;
    eui[1] = 0x00;
    eui[2] = 0x00;
    eui[3] = 0x00;
    eui[4] = 0x00;
    eui[5] = 0x00;
    eui[6] = 0x00;
    eui[7] = 0x01;

    return true;
}

bool config_get_eui48(uint8_t *eui)
{
    eui[0] = 0x00;
    eui[1] = 0x80;
    eui[2] = 0xE1;
    eui[3] = 0x00;
    eui[4] = 0x00;
    eui[5] = 0x00;

    return true;
}


