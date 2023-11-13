/**
 * @file terminal.c
 * @author Joe Bayer (joexbayer)
 * @brief Handles terminal input and currently.. UI drawing.
 * @version 0.1
 * @date 2022-06-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <terminal.h>
#include <args.h>
#include <colors.h>
#include <gfx/gfxlib.h>
#include <pcb.h>
#include <terminal.h>
#include <gfx/theme.h>

void terminal_set_color(color_t color)
{
	current_running->term->text_color = color;
}

void terminal_syntax(unsigned char c)
{
	struct gfx_theme* theme = kernel_gfx_current_theme();

		/* Set different colors for different syntax elements */
	switch (c) {
		case '>':
		case '/':
		case '\\':
			/* Highlight preprocessor directives */
			terminal_set_color(COLOR_VGA_MISC);
			break;
		case '"':
		case ':':
		case '-':
			/* Highlight string literals and character literals */
			terminal_set_color(COLOR_VGA_GREEN);
			break;
		default:
			terminal_set_color(theme->terminal.text);
			break;
	}
}

/**
 * Writes out terminal buffer to screen.
 */
void terminal_commit()
{	
	if(current_running->term == NULL)
		return;

	struct gfx_theme* theme = kernel_gfx_current_theme();
	int x = 0, y = 0;
	kernel_gfx_draw_rectangle(current_running->gfx_window, 0, 0, current_running->term->screen->inner_width, current_running->term->screen->inner_height, theme->terminal.background);
	for (int i = current_running->term->tail; i < current_running->term->head; i++){
		if(current_running->term->textbuffer[i] == '\n'){
			x = 0;
			y++;
			continue;
		}

		terminal_syntax(current_running->term->textbuffer[i]);
		kernel_gfx_draw_char(current_running->gfx_window, 1 + x*8, 1+ y*8, current_running->term->textbuffer[i], current_running->term->text_color);
		x++;
	}
}

void terminal_attach(struct terminal* term)
{
	current_running->term = term;
	term->screen = current_running->gfx_window;
}

int nextNewline(void* _data)
{
	char* data = (char*) _data;

	while (*data != '\n'){
		data++;
	}
	data++;

	return (int)data - (int)_data;
}

void terminal_remove_line(struct terminal* term)
{
	int skip = nextNewline(term->textbuffer);
	memcpy(term->textbuffer, &term->textbuffer[skip], term->head-skip);
	term->head -= skip;
}

void terminal_scroll(struct terminal* term)
{
	/*if(term->tail == term->head)
		return;
		
	while(term->textbuffer[term->tail] != '\n' && term->head != term->tail){
		term->tail++;
	}
	term->tail++;*/

	terminal_remove_line(term);
}

/**
 * Puts a given character to the terminal.
 * @param char c character to put on screen.
 * @return void
 */
void terminal_putchar(char c)
{
	if(current_running->term == NULL || current_running->term->head < 0 || current_running->term->head > TERMINAL_BUFFER_SIZE)
		return;
	
	//unsigned char uc = c;	
	if(c == '\n'){
		if((current_running->term->screen->inner_height/8) -1 == current_running->term->lines)
			terminal_scroll(current_running->term);
		else
			current_running->term->lines++;
	}

	current_running->term->textbuffer[current_running->term->head] = c;
	current_running->term->head++;

	serial_put(c);
	gfx_commit();
}
 
/**
 * Writes the given string to the terminal with terminal_putchar
 * @param char* data to print to screen
 * @param int size of data
 * @return void
 */
void terminal_write(const char* data, int size)
{
	if(current_running->term == NULL)
		return;

	for (int i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

/**
 * Writes the given string to the terminal.
 * @param char* data to print to screen
 * @see terminal_write
 * @return void
 */
void twrite(const char* data)
{
	if(current_running->term == NULL)
		return 0;
	terminal_write(data, strlen(data));
}

void twriteln(const char* data)
{
	if(current_running->term == NULL)
		return 0;
	twrite(data);
	terminal_putchar('\n');
}

#define MAX_FMT_STR_SIZE 50

int32_t twritef(char* fmt, ...)
{
	if(current_running->term == NULL)
		return 0;
	
	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[MAX_FMT_STR_SIZE];
	int num = 0;

	va_start(args, fmt);

	while (*fmt != '\0') {
		switch (*fmt){
			case '%':
				memset(str, 0, MAX_FMT_STR_SIZE);
				switch (*(fmt+1))
				{
					case 'd': ;
						num = va_arg(args, int);
						itoa(num, str);
						twrite(str);
						x_offset += strlen(str);
						break;
					case 'i': ;
						num = va_arg(args, int);
						unsigned char bytes[4];
						bytes[0] = (num >> 24) & 0xFF;
						bytes[1] = (num >> 16) & 0xFF;
						bytes[2] = (num >> 8) & 0xFF;
						bytes[3] = num & 0xFF;
						twritef("%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
						break;
					case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);

						if(strlen(str) < 5){
                            int pad = 5-strlen(str);
                            for (int i = 0; i < pad; i++){
                                terminal_putchar('0');
                            }
                        }

						twrite(str);
						x_offset += strlen(str);
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						twrite(str);
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						twrite(str_arg);
						x_offset += strlen(str_arg);
						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						terminal_putchar(char_arg);
						x_offset++;
						break;
					
					default:
						terminal_putchar(*fmt);
						x_offset++;
						break;
				}
				fmt++;
				break;
			default:  
				terminal_putchar(*fmt);
				x_offset++;
			}
        fmt++;
    }
	written += x_offset;
	return written;
}
