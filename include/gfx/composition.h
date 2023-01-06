#ifndef FCA672BC_C2FD_4772_BC32_C01EF99BEA47
#define FCA672BC_C2FD_4772_BC32_C01EF99BEA47

#include <gfx/window.h>

void gfx_composition_add_window(struct gfx_window* w);
void gfx_composition_remove_window(struct gfx_window* w);

void gfx_compositor_main();
void gfx_mouse_event(int x, int y, char flags);

#endif /* FCA672BC_C2FD_4772_BC32_C01EF99BEA47 */