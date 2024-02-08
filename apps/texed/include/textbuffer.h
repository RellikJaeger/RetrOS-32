#ifndef __TEXTBUFFER_H
#define __TEXTBUFFER_H

#include <colors.h>
#include <stdint.h>

typedef enum __line_flags {
	LINE_FLAG_NONE = 1 << 0,
	LINE_FLAG_DIRTY = 1 << 1,
	LINE_FLAG_EXTENSION = 1 << 2,
} line_flags_t;

struct textbuffer {
	struct textbuffer_ops {
		int (*destroy)(struct textbuffer *buffer);
		int (*display)(const struct textbuffer *buffer, enum vga_color fg, enum vga_color bg);
		int (*put)(struct textbuffer *buffer, unsigned char c);
		int (*jump)(struct textbuffer *buffer, size_t x, size_t y);
	} *ops;
	struct line {
		char *text;
		size_t length;
		size_t capacity;
		line_flags_t flags;
	} **lines;
	struct cursor {
		unsigned int x;
		unsigned int y;
	} cursor;
	struct scroll {
		size_t start;
		size_t end;
	} scroll;
	size_t line_count;
	char filename[256];
};

int textbuffer_search_main(struct textbuffer* buffer);

#endif // !__TEXTBUFFER_H