#include <stdarg.h>
#include "uart.h"

static void print_num(unsigned long num, int base) {
    char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;

    if (num == 0) {
        uart_putc('0');
        return;
    }

    while (num > 0) {
        buf[i++] = digits[num % base];
        num /= base;
    }

    while (i--) {
        uart_putc(buf[i]);
    }
}

void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++; // skip '%'

        switch (*p) {
        case 'c':
            uart_putc((char)va_arg(args, int));
            break;
        case 's':
            uart_puts(va_arg(args, char *));
            break;
        case 'd':
        case 'i':
            print_num(va_arg(args, int), 10);
            break;
        case 'x':
            print_num(va_arg(args, unsigned int), 16);
            break;
        case 'l':
            p++; // skip 'l'
            if (*p == 'x') {
                print_num(va_arg(args, unsigned long), 16);
            } else if (*p == 'd') {
                print_num(va_arg(args, unsigned long), 10);
            } else {
                uart_putc('%');
                uart_putc('l');
                uart_putc(*p);
            }
            break;
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('%');
            uart_putc(*p);
            break;
        }
    }

    va_end(args);
}
