#include "net_driver.h"
#include "printk.h"
#include <stddef.h>

int net_send_packet(const uint8_t *data, uint16_t len) {
    return net_send(data, len);
}
