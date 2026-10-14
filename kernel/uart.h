#ifndef UART_H
#define UART_H

#define UART_BUFFER_SIZE 256

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *s);
int uart_has_data(void);
void uart_interrupt_handler(void);

#endif
