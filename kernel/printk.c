#include <stdarg.h>
#include "uart.h"
#include "irq.h"

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

/* Print num in given base, zero-padded to min_width characters. */
static void print_num_padded(unsigned long num, int base, int min_width) {
    char digits[] = "0123456789abcdef";
    char buf[32];
    int i = 0;
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = digits[num % base];
            num /= base;
        }
    }
    while (i < min_width) {
        uart_putc('0');
        min_width--;
    }
    while (i--) {
        uart_putc(buf[i]);
    }
}

void printk(const char *fmt, ...) {
    irq_state_t irq = irq_save();
    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++; // skip '%'

        // Handle precision specifier for strings (%.*s)
        if (*p == '.' && *(p+1) == '*' && *(p+2) == 's') {
            int width = va_arg(args, int);
            char *str = va_arg(args, char *);
            
            // Print up to 'width' characters from the string
            for (int i = 0; i < width && str && str[i]; i++) {
                uart_putc(str[i]);
            }
            p += 2; // skip ".*s" (we already skipped %)
            continue;
        }

        /* Handle %0Nx and %0Nd zero-padded width (single digit N). */
        if (*p == '0' && p[1] >= '1' && p[1] <= '9') {
            int width = p[1] - '0';
            char kind = p[2];
            if (kind == 'x') {
                print_num_padded(va_arg(args, unsigned int), 16, width);
                p += 2;
                continue;
            } else if (kind == 'd' || kind == 'u') {
                print_num_padded(va_arg(args, unsigned int), 10, width);
                p += 2;
                continue;
            }
        }

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
        case 'u':
            print_num(va_arg(args, unsigned int), 10);
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
    irq_restore(irq);
}
