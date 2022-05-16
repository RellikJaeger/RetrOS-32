#include "util.h"
#include "pci.h"
#include "terminal.h"
#include "interrupts.h"
#include "timer.h"

void _main(uint32_t debug) {
    /* Initialize terminal interface */
	terminal_initialize();
	init_interrupts();
	init_timer(1);

	if(debug == 0xDEADBEEF)
	{
		twrite("Hello world\n");
	}
	scrwrite(1, 1, "Running... !");

	int test_int = 0;
	/* Testing printing ints and hex */
	char test[1000];
	itohex(3735928559, test);
	twrite(test);
	twrite("\n");

	/* Testing PCI */
	int dev = pci_find_device(0x8086, 0x100E);
	if(dev){
		twrite("PCI Device 0x100E Found!\n");
	}

	/* Test interrupt */
	asm volatile ("int $32");
	//asm volatile ("int $31");

	while(1){
		char test[1000];
		itoa(test_int, test);
		twrite(test);
		twrite("\n");
		test_int = (test_int+1) % 10000;
	};

}