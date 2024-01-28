/**
 * @file printf.c
 * @author Joe Bayer (joexbayer)
 * @brief printf implementation userspace mirror of terminal.c
 * @version 0.1
 * @date 2022-08-21
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <args.h>
#include <lib/syscall.h>
#include <libc.h>

/**
 * Writes the given string to the terminal with terminal_putchar
 * @param char* data to print to screen
 * @param int size of data
 * @return void
 */
void print_write(const char* data, int size)
{
	for (int i = 0; i < size; i++)
		print_put(data[i]);
}

/**
 * Writes the given string to the terminal.
 * @param char* data to print to screen
 * @see terminal_write
 * @return void
 */
void print(const char* data)
{
	print_write(data, strlen(data));
}

void println(const char* data)
{
	print(data);
	print_put('\n');
}

#define MAX_FMT_STR_SIZE 256

int printf(const char* fmt, ...)
{
	va_list args;

	int x_offset = 0;
	int written = 0;
	char str[MAX_FMT_STR_SIZE];
	int num = 0;

	va_start(args, fmt);

	while (*fmt != '\0') {
		switch (*fmt)
		{
			case '%':
				memset(str, 0, MAX_FMT_STR_SIZE);
				switch (*(fmt+1))
				{
					case 'd': ;
						num = va_arg(args, int);
						itoa(num, str);
						print(str);
						x_offset += strlen(str);
						break;
					case 'i': ;
						num = va_arg(args, int);
						unsigned char bytes[4];
						bytes[0] = (num >> 24) & 0xFF;
						bytes[1] = (num >> 16) & 0xFF;
						bytes[2] = (num >> 8) & 0xFF;
						bytes[3] = num & 0xFF;
						printf("%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
						break;
					case 'p': ; /* p for padded int */
						num = va_arg(args, int);
						itoa(num, str);
						print(str);
						x_offset += strlen(str);

                        if(strlen(str) < 3){
                            int pad = 3-strlen(str);
                            for (int i = 0; i < pad; i++){
                                print_put(' ');
                            }
                        }
						break;
					case 'x':
					case 'X': ;
						num = va_arg(args, int);
						itohex(num, str);
						print(str);
						x_offset += strlen(str);
						break;
					case 's': ;
						char* str_arg = va_arg(args, char *);
						print(str_arg);
						x_offset += strlen(str_arg);
						break;
					case 'c': ;
						char char_arg = (char)va_arg(args, int);
						print_put(char_arg);
						x_offset++;
						break;
					
					default:
						break;
				}
				fmt++;
				break;
			default:  
				print_put(*fmt);
				x_offset++;
			}
        fmt++;
    }
	written += x_offset;
	return written;
}

/* Custom sprintf function */
int sprintf(char *buffer, const char *fmt, va_list args)
{
    int written = 0; /* Number of characters written */
    char str[MAX_FMT_STR_SIZE];
    int num = 0;

    while (*fmt != '\0' && written < MAX_FMT_STR_SIZE) {
        if (*fmt == '%') {
            memset(str, 0, MAX_FMT_STR_SIZE); /* Clear the buffer */
            fmt++; /* Move to the format specifier */

            if (written < MAX_FMT_STR_SIZE - 1) {
                switch (*fmt) {
                    case 'd':
                    case 'i':
                        num = va_arg(args, int);
                        itoa(num, str);
                        break;
                    case 'x':
                    case 'X':
                        num = va_arg(args, unsigned int);
                        written += itohex(num, str);
                        break;
                    case 'p': /* p for padded int */
                        num = va_arg(args, int);
                        itoa(num, str);

                        if (strlen(str) < 5) {
                            int pad = 5 - strlen(str);
                            for (int i = 0; i < pad; i++) {
                                buffer[written++] = '0';
                            }
                        }
                        break;
                    case 's':{
                            char *str_arg = va_arg(args, char*);
                            while (*str_arg != '\0' && written < MAX_FMT_STR_SIZE - 1) {
                                buffer[written++] = *str_arg++;
                            }
                        }
                        break;
                    case 'c':
                        if (written < MAX_FMT_STR_SIZE - 1) {
                            buffer[written++] = (char)va_arg(args, int);
                        }
                        break;
                    /* Add additional format specifiers as needed */
                }

                /* Copy formatted string to buffer */
                for (int i = 0; str[i] != '\0'; i++) {
                    buffer[written++] = str[i];
                }
            }
        } else {
            /* Directly copy characters that are not format specifiers */
            if (written < MAX_FMT_STR_SIZE - 1) {
                buffer[written++] = *fmt;
            }
        }
        fmt++;
    }

    /* Ensure the buffer is null-terminated */
    buffer[written < MAX_FMT_STR_SIZE ? written : MAX_FMT_STR_SIZE - 1] = '\0';

    return written;
}

#ifdef __cplusplus
}
#endif
