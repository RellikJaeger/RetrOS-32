#include <lib/graphics.h>
#include <lib/syscall.h>
#include <util.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

int gfx_draw_char(int x, int y, char data, unsigned char color)
{
    struct gfx_char c = {
        .color = color,
        .data = data,
        .x = x,
        .y = y
    };
    gfx_draw_syscall(GFX_DRAW_CHAR_OPT, &c);

    return 0;
}

int gfx_draw_circle(int x, int y, int r, unsigned char color)
{
	struct gfx_circle c = {
		.x = x,
		.y = y,
		.r = r,
		.color = color
	};

	gfx_draw_syscall(GFX_DRAW_CIRCLE_OPT, &c);

	return 0;
}

int gfx_draw_line(int x0, int y0, int x1, int y1, unsigned char color)
{
	struct gfx_line line = {
		.x0 = x0,
		.y0 = y0,
		.y1 = y1,
		.x1 = x1,
		.color = color
	};

	gfx_draw_syscall(GFX_DRAW_LINE_OPT, &line);

	return 0;
}

int gfx_draw_rectangle(int x, int y, int width, int height, unsigned char color)
{
    struct gfx_rectangle rect = {
		.color = color,
		.x = x,
		.y = y,
		.width = width,
		.height = height
	};

    gfx_draw_syscall(GFX_DRAW_RECTANGLE_OPT, &rect);

    return 0;
}

int gfx_draw_text(int x, int y, char* text, unsigned char color)
{
    struct gfx_char c = {
        .color = color,
        .data = 0,
        .x = x,
        .y = y
    };

    int len = strlen(text);
    for (int i = 0; i < len; i++)
    {
        c.data = text[i];
        gfx_draw_syscall(GFX_DRAW_CHAR_OPT, &c);
        c.x += 8;
    }
    
    return 0;
}

int gfx_get_event(struct gfx_event* event)
{
	return gfx_draw_syscall(GFX_EVEN_LOOP_OPT, event);
}

#define GFX_MAX_FMT 50
int gfx_draw_format_text(int x, int y, char color, char* fmt, ...)
{
	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[GFX_MAX_FMT];
	int num = 0;

	va_start(args, fmt);

	while (*fmt != '\0') {
		switch (*fmt)
		{
			case '%':
				memset(str, 0, GFX_MAX_FMT);
				switch (*(fmt+1))
				{
					case 'd':
					case 'i': ;
						num = va_arg(args, int);
						itoa(num, str);
						gfx_draw_text(x+(x_offset*8), y, str, color);
						x_offset += strlen(str);
						break;
                    case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);
						gfx_draw_text(x+(x_offset*8), y, str, color);
						x_offset += strlen(str);

                        if(strlen(str) < 3){
                            int pad = 3-strlen(str);
                            for (int i = 0; i < pad; i++){
                                gfx_draw_char(x+(x_offset*8), y, ' ', color);
                                x_offset++;
                            }
                        }
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						gfx_draw_text(x+(x_offset*8), y, str, color);
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						gfx_draw_text(x+(x_offset*8), y, str_arg, color);
						x_offset += strlen(str_arg);
						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						gfx_draw_char(x+(x_offset*8), y, char_arg, color);
						x_offset++;
						break;
					default:
                        break;
				}
				fmt++;
				break;
			case '\n':
				y++;
				written += x_offset;
				x_offset = 0;
				break;
			default:  
				gfx_draw_char(x+(x_offset*8), y, *fmt, color);
				x_offset++;
                written++;
			}
        fmt++;
    }
	written += x_offset;
	return written;
}

#ifdef __cplusplus
}
#endif