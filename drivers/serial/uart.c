#include <junkv/types.h>
#include <platform.h>

/*
 * The UART control registers are memory-mapped at address UART0. 
 * This macro returns the address of one of the registers.
 */
#define UART_REG(reg) ((volatile uint8_t *)(UART0 + reg))

#define uart_read_reg(reg) (*(UART_REG(reg)))
#define uart_write_reg(reg, v) (*(UART_REG(reg)) = (v))

#define LSR_RX_READY (1 << 0)
#define LSR_TX_IDLE  (1 << 5)

/*
 * Reference
 * [1]: TECHNICAL DATA ON 16550, man/td16650.pdf
 */

/*
 * UART control registers map. see [1] "PROGRAMMING TABLE"
 * 0 (write mode): THR/DLL
 * 1 (write mode): IER/DLM
 */
#define RHR 0	// read  mode: Receive Holding Register
#define THR 0	// write mode: Transmit Holding Register
#define DLL 0	// write mode: LSB of Divisor Latch when Enabled
#define IER 1	// write mode: Interrupt Enable Register
#define DLM 1	// write mode: MSB of Divisor Latch when Enabled
#define ISR 2	// read  mode: Interrupt Status Register
#define FCR 2	// write mode: FIFO Control Register
#define LCR 3	// write mode: Line Control Register
#define MCR 4	// write mode: Modem Control Register
#define LSR 5	// read  mode: Line Status Register
#define MSR 6	// read  mode: Modem Status Register
#define SPR 7	// ScratchPad Register

/*
 * POWER UP DEFAULTS
 * IER = 0: TX/RX holding register interrupts are both disabled
 * ISR = 1: no interrupt penting
 * LCR = 0
 * MCR = 0
 * LSR = 60 HEX
 * MSR = BITS 0-3 = 0, BITS 4-7 = inputs
 * FCR = 0
 * TX = High
 * OP1 = High
 * OP2 = High
 * RTS = High
 * DTR = High
 * RXRDY = High
 * TXRDY = Low
 * INT = Low
 */

/*
 * LINE STATUS REGISTER (LSR)
 * LSR BIT 0:
 * 0 = no data in receive holding register or FIFO.
 * 1 = data has been receive and saved in the receive holding register or FIFO.
 * ......
 * LSR BIT 5:
 * 0 = transmit holding register is full. 16550 will not accept any data for transmission.
 * 1 = transmitter hold register (or FIFO) is empty. CPU can load the next character.
 * ......
 */

void uart_init()
{
	/* disable interrupts. */
	uart_write_reg(IER, 0x00);

	/*
	 * Setting baud rate. Just a demo here if we care about the divisor,
	 * but for our purpose [QEMU-virt], this doesn't really do anything.
	 *
	 * Notice that the divisor register DLL (divisor latch least) and DLM (divisor
	 * latch most) have the same base address as the receiver/transmitter and the
	 * interrupt enable register. To change what the base address points to, we
	 * open the "divisor latch" by writing 1 into the Divisor Latch Access Bit
	 * (DLAB), which is bit index 7 of the Line Control Register (LCR).
	 *
	 * Regarding the baud rate value, see [1] "BAUD RATE GENERATOR PROGRAMMING TABLE".
	 * We use 38.4K when 1.8432 MHZ crystal, so the corresponding value is 3.
	 * And due to the divisor register is two bytes (16 bits), so we need to
	 * split the value of 3(0x0003) into two bytes, DLL stores the low byte,
	 * DLM stores the high byte.
	 */
	uint8_t lcr = uart_read_reg(LCR);
	uart_write_reg(LCR, lcr | (1 << 7));
	uart_write_reg(DLL, 0x03);
	uart_write_reg(DLM, 0x00);

	/*
	 * Continue setting the asynchronous data communication format.
	 * - number of the word length: 8 bits
	 * - number of stop bits：1 bit when word length is 8 bits
	 * - no parity
	 * - no break control
	 * - disabled baud latch
	 */
	lcr = 0;
	uart_write_reg(LCR, lcr | (3 << 0));

	/*
	 * enable receive interrupts.
	 */
	uint8_t ier = uart_read_reg(IER);
	uart_write_reg(IER, ier | (1 << 0));
}

void uart_putc(char ch)
{
	while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);
	uart_write_reg(THR, ch);
}

void uart_puts(char *s)
{
	while (*s) {
		uart_putc(*s++);
	}
}

char uart_getc()
{
	while ((uart_read_reg(LSR) & LSR_RX_READY) == 0);
	return uart_read_reg(RHR);
}

uint64_t uart_gets(char *s)
{
	char *t = s;
	while (1) {
		char ch = uart_getc();
		if (ch == '\r') {
			uart_putc('\r');
			uart_putc('\n');
			*t = '\0';
			break;
		} else if (ch == '\b' || ch == 0x7f) {
			if (t > s) {
				uart_puts("\b \b");
				t--;
			}
		} else {
			uart_putc(ch);
			*t++ = ch;
		}
	}
	return t - s;
}
