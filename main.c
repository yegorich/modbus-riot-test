#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ringbuffer.h"
#include "periph/uart.h"
#include "stdio_uart.h"
#include "xtimer.h"

#define UART_BUFSIZE        (128U)
#define PRINTER_PRIO        (THREAD_PRIORITY_MAIN - 1)
#define PRINTER_TYPE        (0xabcd)
#define REF_PACKET          ":01030A00000000000000000000F2\r\n"
typedef struct {
    char rx_mem[UART_BUFSIZE];
    ringbuffer_t rx_buf;
} uart_ctx_t;

static uart_ctx_t ctx[UART_NUMOF];

static kernel_pid_t printer_pid;
static char printer_stack[THREAD_STACKSIZE_MAIN];

static void *printer(void *arg)
{
    (void)arg;
    msg_t msg;
    msg_t msg_queue[8];
    msg_init_queue(msg_queue, 8);
    int msg_counter = 0;
    int err_counter = 0;
    char buf[UART_BUFSIZE];

    while (1) {
        msg_receive(&msg);
        uart_t dev = (uart_t)msg.content.value;
        char c;
        int i = 0;

        printf("RX: [");
        do {
            c = (int)ringbuffer_get_one(&(ctx[dev].rx_buf));
            buf[i++] = c;
            if (c == '\n') {
                puts("]\\n");
            }
            else if (c >= ' ' && c <= '~') {
                printf("%c", c);
            }
            else {
                printf("0x%02x", (unsigned char)c);
            }
        } while (c != '\n');
        buf[i] = '\0';
	msg_counter++;
        if (strncmp(buf, REF_PACKET, strlen(REF_PACKET)) != 0) {
            printf("Packet does not match the reference one\n");
	    err_counter++;
        }
	printf("Total message counter: %d\n", msg_counter);
	printf("Error counter: %d\n", err_counter);
    }

    /* this should never be reached */
    return NULL;
}

static void rx_cb(void *arg, uint8_t data)
{
    uart_t dev = (uart_t)arg;

    ringbuffer_add_one(&(ctx[dev].rx_buf), data);
    if (data == '\n') {
        msg_t msg;
        msg.content.value = (uint32_t)dev;
        msg_send(&msg, printer_pid);
    }
}

int main(void)
{
    int i, dev = 1;
    puts("modbus tester!");

    char ascii_packet[64] = ":01030063000594\r\n";

    /* initialize ringbuffers */
    for (unsigned i = 0; i < UART_NUMOF; i++) {
        ringbuffer_init(&(ctx[i].rx_buf), ctx[i].rx_mem, UART_BUFSIZE);
    }

    /* start the printer thread */
    printer_pid = thread_create(printer_stack, sizeof(printer_stack),
                                PRINTER_PRIO, 0, printer, NULL, "printer");
    if (uart_init(UART_DEV(dev), 115200, rx_cb, (void *)dev) != 0) {
        puts("UART init failed\n");
        return 1;
    }

    if (uart_mode(UART_DEV(dev), UART_DATA_BITS_7, UART_PARITY_EVEN, UART_STOP_BITS_1) != UART_OK) {
        puts("Error: Unable to apply UART settings");
        return 1;
    }

    puts("Sending string\n");
    for (i = 0; i < 100; i++) {
    	uart_write(UART_DEV(dev), (uint8_t *)ascii_packet, strlen(ascii_packet));
        xtimer_sleep(1);
    }

    while(1) {
        xtimer_sleep(1);
    }

    return 0;
}
