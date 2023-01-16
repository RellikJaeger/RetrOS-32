#ifndef KTHREADS_H
#define KTHREADS_H

#include <stdint.h>

/* kthreads functions: FIXME */
void shell_main();
void networking_main();
void dhcpd();
void error_main();
void gfx_window_debugger();

int start(char* name);
int register_kthread(void (*f)(), char* name);

#endif /* KTHREADS_H */
