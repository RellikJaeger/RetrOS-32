#include "timer.h"
#include "terminal.h"
#include "interrupts.h"

#define PIT_IRQ		32

static int tick = 0;
static void timer_callback()
{
	tick++;
	char test[1000];
	itoa(tick % 1000, test);
	scrwrite(10, 10, test);
}

void init_timer(uint32_t frequency)
{
	// Firstly, register our timer callback.
	isr_install(PIT_IRQ, &timer_callback);

	// The value we send to the PIT is the value to divide it's input clock
	// (1193180 Hz) by, to get our required frequency.
	uint32_t divisor = 1193180 / frequency;

	// Send the command byte.
	outportb(0x43, 0x36);

	// Divisor has to be sent byte-wise, so split here into upper/lower bytes.
	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

	// Send the frequency divisor.
	outportb(0x40, l);
	outportb(0x40, h);

	twrite("Timer Started.\n");
    STI();
}