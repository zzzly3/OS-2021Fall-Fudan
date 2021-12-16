#pragma once

#ifndef _DRIVER_UART_H_
#define _DRIVER_UART_H_

#include <common/defines.h>
#include <common/lutil.h>

void init_uart();
char uart_get_char();
void uart_put_char(char c);
BOOL uart_valid_char(const char c);
void uart_intr();

#endif
