#include <core/char_device.h>
#include <driver/uart.h>

void init_char_device() {
    init_uart();
}

void init_uart_char_device(CharDevice *device) {
    device->get = uart_get_char;
    device->put = uart_put_char;
}

void free_range(void *start, void *end) {
    return;
}
void *kalloc(void) {
    return NULL;
}
void kfree(void *page_address) {
    return;
}