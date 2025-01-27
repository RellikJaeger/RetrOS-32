#ifndef FCA672BC_C2FD_4772_BC32_C01EF99BEA47
#define FCA672BC_C2FD_4772_BC32_C01EF99BEA47

#include <gfx/window.h>

void gfx_composition_add_window(struct window* w);
void gfx_composition_remove_window(struct window* w);

int gfx_decode_background_image(const char* file);

void gfx_compositor_main();
void gfx_set_fullscreen(struct window* w);
int gfx_raw_background(char* path);
int gfx_set_background_color(color_t color);

int gfx_set_taskbar(pid_t pid);

#endif /* FCA672BC_C2FD_4772_BC32_C01EF99BEA47 */
