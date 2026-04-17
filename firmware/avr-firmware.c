

#include <avr/io.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <avr/pgmspace.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

void pf(const char *msg, ...);
  
#include "gpio.h"
#include "pins.h"
#include "pin_defaults.h"

#include "git-rev.h"
#include "build-info.h"

const char build_info[] = "BUILD: " BUILD_TYPE "\n" \
  "BUILD-BY: " BUILD_BY "\n" \
  "GIT-VER: " GIT_VERSION "\n" \
  "GIT-SHA: " GIT_HEAD "\n";

static void init_pwm(void)
{
	//TCCR2A = (2 << 4) /* OC2B = normal) */ | (2 << 0) /* CTC */;
	//TCCR2B = (1); /* no prescaler */

	TCCR2A = (2 << 4) | (1); /* for phase correct pwm */
	TCCR2B = (1) | (1 << 3);

	OCR2A = 0x10;
	OCR2B = 0x5;
}

static unsigned char rom[32] = {
	/* download code helper */
	0xA9, 0x00,		/* LDA #0x00 */
	0x8D, 0x00, 0x00,	/* STA 0x0000 */
	0x90, 0xF9,		/* BCC start */
	0x4C, 0xCD, 0xAB,	/* JMP 0xABCD */
};

static unsigned char halted = 0;

ISR(TIMER1_COMPA_vect)
{
	if (!halted)
		toggle_pin(PIN_AT_LED);
}

/* set the clock divider post initialisation. The fuses only allow /1 or /8
 * and if we're using the 18.432 MHz option then the /2 option is probably
 * the best.
 */
static void init_clock(void)
{
	if (F_CPU == F_XTAL) {
		CLKPR = 0 | (1 << CLKPCE);
		CLKPR = 0;
	} else if (F_CPU == (F_XTAL / 2)) {
		CLKPR = 0 | (1 << CLKPCE);
		CLKPR = 1;
	} else {
		extern void __bad_crystal(void);
		__bad_crystal();	/* default to compiler link error */
	}

	/* wait for clock change to take place */
	while (CLKPR & (1 << CLKPCE)) { }
}

#define USART_BAUDRATE	115200
#define BAUD_PRESCALE	(((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

 void init_uart(void)
{
	UBRR0L = BAUD_PRESCALE;
	UBRR0H = (BAUD_PRESCALE >> 8);

	UCSR0C = (1<<UCSZ00)|(1<<UCSZ01);		/* set to 8n1 */
	UCSR0B = (1 << TXEN0)| (1 << RXEN0);	/* enable rx/tx */
	//UCSR0B |= (1 << RXCIE) | (1 << TXCIE) | (1 << UDRIE);	/* irq enable */
}


/* uart handling routines */
static unsigned uart_hasbyte(void)
{
	return (UCSR0A & (1 << RXC0));
}

static uint8_t uart_getbyte(void)
{
	return UDR0;
}

static void uart_putbyte(uint8_t byte)
{
	while( (UCSR0A & (1<<UDRE0)) == 0) { }	/* wait for uart empty */
	UDR0 = byte;
}

static void uart_putc(uint8_t byte)
{
	if (byte == '\n')
		uart_putbyte('\r');
	uart_putbyte(byte);
}

/* simple printf-like routine with small buffer */
void pf(const char *msg, ...)
{
	va_list va;
	char buff[40];
	int off, len;

	va_start(va, msg);
	len = vsnprintf(buff, sizeof(buff), msg, va);
	va_end(va);

	for (off = 0; off < len; off++)
		uart_putc(buff[off]);
}

/* hand assembled 6502 for doing stuff */

const unsigned char download_code[] PROGMEM = {
	/*

*=$c000
LDX #$00
loop STX $801E
INX
BCC loop
BRK
.END
	*/
	0x18,
	0xa2, 0x00,
	0x8e, 0x1e, 0x80,
	0xe8,
	0x90, 0xfa,
	0x00,
};

#include "target_asm/test_str.th"

static const unsigned char ramtest_code_read[] PROGMEM = {
/*
*=$c000
//CLC
loop
LDX $0123
STX $801F
BCC loop
.END
*/
	//0x18,
	0xae, 0x23, 0x01,
	0x8e, 0x1f, 0x80,
	0x90, 0xf8,
};

#if 1
const unsigned char *download_ptr = download_code;
unsigned download = sizeof(download_code);
#endif

#if 0
const unsigned char *download_ptr = test_str_bin;
unsigned download = sizeof(test_str_bin);
#endif

unsigned download_to = 0x800;

static const unsigned ram_sz = 8 * 1024;
static unsigned ram_test_ptr = 0x0000;
static unsigned ram_test = 0;

static unsigned ram_test_pattern(unsigned addr)
{
	// really simple ram test pattern
	return (addr ^ (addr >> 8));
}

static void halt(void)
{
	pf("HALT\n");
	set_pin(PIN_6502_nRESET, 0);
	halted = 1;
	PORTD &= ~(1 << 7);		
}

static void ram_test_isr_code(unsigned addr)
{
	unsigned int ptr;

	/* assume addr has been checked and is 1 */

	if (ram_test_ptr >= ram_sz) {
		ram_test++;
		ram_test_ptr = 0x0;

		switch (ram_test) {
		case 2:
			pf("RAM test - patterns written\n");

			for (ptr = 0x00; ptr < 0x10; ptr++)
				rom[ptr] = pgm_read_byte(ramtest_code_read + ptr);

			PORTA = rom[1];
			break;
		case 3:
			pf("RAM test done\n");
			halt();
			/* done */
		}
	} else if (ram_test_ptr == 0x00) {
		pf("RAM test - pass %d\n", ram_test);
	} else if ((ram_test_ptr & 0xff) == 0) {
		pf(".");
	}

	switch (ram_test) {
	case 1:
		rom[1] = ram_test_pattern(ram_test_ptr);
		rom[3] = ram_test_ptr & 0xff;
		rom[4] = ram_test_ptr >> 8;
		ram_test_ptr++;
		break;
	case 2:
		rom[1] = ram_test_ptr & 0xff;
		rom[2] = ram_test_ptr >> 8;
		break;
	}

}

static unsigned dump_mem;
static unsigned count = 0;

// seem to be seeing double execution of the CLC instruction at 0x00
// but with two cycles, the first is addr, adn then with addr+1

/* update the download code ram when addr+0x0 is read so that the
 * download data is there and if the download is finished we continue
 * on past the BCC loop
 */
static inline void update_download_state(void)
{
	rom[1] = pgm_read_byte(download_ptr);
	rom[3] = download_to & 0xff;
	rom[4] = download_to >> 8;
	pf("DL %04x %02x (C=%d)\n", download_to, rom[1], count);

	download_to++;
	download_ptr++;
	download--;

	if (download == 0) {
		pf("Download done\n");
		rom[5] = 0xb0;  /* change BCC to BCS */
	}
}

/* device is reading from emulated rom, so we need to return a value on the
 * data lines depending on the address
 */
static inline void handle_rom_read(unsigned addr)
{
	PORTA = rom[addr];

	if (addr == 0x1e && false) {
		unsigned ptr;
		pf("BRK!\n");

		for (ptr = 0x00; ptr < 0x10; ptr++)
			rom[ptr] = pgm_read_byte(ramtest_code_read + ptr);

		PORTA = rom[addr];
		download_to = 0x1fc;
		dump_mem  = 1;
		download = 0;
		ram_test = 0;
		rom[5] = 0x1d;  // change where memory is written to
	}

	if (download) {
		if (addr == 0)
			update_download_state();
	} else if (ram_test) {
		if (addr == 0)
			ram_test_isr_code(addr);

	} else if (dump_mem) {
		if (addr == 0) {
			pf("DBG %04x = ", download_to);
			rom[1] = download_to & 0xff;
			rom[2] = download_to >> 8;
			download_to++;
			if (download_to == 0x200)
				download_to = 0x800;
			if (download_to > 0x810) {
				halt();
			}
		}

	}

	if (1) {
		pf("RR: A=%02x => %02x\n", addr, PORTA);
	}
}

static void handle_dev_write(unsigned addr, unsigned data)
{
	unsigned tmp;

	switch (addr) {
	case 0x1c:
		pf("%c", data);
		break;
	case 0x1d:
		pf("%02x\n", data);
		break;
	case 0x1e:
		pf("DBG %02x\n", data);
		break;

	case 0x1f:
		tmp =  ram_test_pattern(ram_test_ptr) & 0xff;
		if (data == tmp) {
			// good, ignore //
		} else {
			pf("RT %04x -> got %02x want %02x, diff %02x\n",
			   ram_test_ptr, data, tmp, data ^ tmp);
			_delay_ms(100);
		}
		ram_test_ptr++;
		break;

	default:
		if (1)
			pf("DW unhandled %02x %02x\n", addr, data);
	}
}

ISR(INT2_vect)
{
	unsigned portc = PINC;
	unsigned addr = (portc >> 2) & 0x1f;
	unsigned read = PINB & (1 << 1);
	unsigned is_rom = (portc & (1 << 7));

	/* PB0 is high for read
	 * PC7 being 0 means selelected as IO device */

	if (read) {
		if (!is_rom) {
			pf("RD %02x = %02x C=%u\n", addr, rom[addr], count);
		} else {
			if (0 || (addr >= 0x1e) || dump_mem)
				pf("RD %02x = %02x C=%u\n", addr, rom[addr], count);

			handle_rom_read(addr);
		}
	} else {
		unsigned data;

		/* handle write to device */

		DDRA = 0x00;
		_delay_us(1);
		data = PINA;
		DDRA = 0xff;

		if (0)
			pf("WR %02x %02x\n", addr, data);

		if (!is_rom) {
			handle_dev_write(addr, data);
		} else {
			pf("WR %02x %02x\n", addr, data);
		}
	}

	/* pulse ack pin to let the 6502 continue */
	set_pin(PIN_AT_ACK, 0);
	set_pin(PIN_AT_ACK, 1);

	count++;
	if (count > 128 && false)
		halt();
}

int main(void)
{

	cli();
	wdt_disable();

	count = 0;

	/* initialise our virtual ram/rom */
	rom[0xFC & 0x1f] = 0x00;
	rom[0xFD & 0x1f] = 0xC0;  // set "rom" as reset vector
	rom[0xFE & 0x1f] = 0x00;
	rom[0xFF & 0x1f] = 0xC0;  // set "rom" as break vector
	rom[0xFE & 0x1f] = 0x18;
	rom[0xFF & 0x1f] = 0xC0;  // set "rom" as nmi vector

	init_gpio();
	init_clock();
	init_pwm();
	init_uart();

	sei();

	init_uart();
	if (1) {
	  pf("6502Atty (" GIT_VERSION ")\n");
	}
	
	/* prototype needs to release master reset */
	_delay_ms(100);
#ifdef PIN_MAIN_RESET
	set_pin(PIN_MAIN_RESET, 1);
#endif

	/* enable interrupts */

	EICRA = (1 << ISC21) | (0 << ISC20);
	EIMSK = (1 << 2);

#if 1
	/* timer to toggle leds for testing */

	TCCR1B |= (1 << WGM12); // Configure timer 1 for CTC mode
        OCR1A = 1000000000/(F_CPU/256);        // Set CTC compare value to 1Hz
        TCCR1B |= (1 << CS12);  // Start timer at Fcpu/256

        TIMSK1 = (1 << OCIE1A);
#endif	

	pf("\nstarting 6502..\n");

	/* pulse ack pin to reset the nCPU_STOP signal */
	set_pin(PIN_AT_ACK, 0);
	set_pin(PIN_AT_ACK, 1);

	if (download) {
		rom[8] = download_to & 0xff;
		rom[9] = download_to >> 8;
	}

	if (false) {
		/* we can get around 2.0MHz output from this with around
		 * 55-45% high low */
		pf("Test pattern on LED\n");
		while (true) {
			set_pin(PIN_AT_LED, 0);
			set_pin(PIN_AT_LED, 1);
		}
	}

	_delay_ms(100);
	pf("GO\n");

	set_pin(PIN_6502_nRESET, 1);	/* release 6502 reset */
	while (1) { }
}
