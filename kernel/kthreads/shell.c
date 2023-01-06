/**
 * @file shell.c
 * @author Joe Bayer (joexbayer)
 * @brief Simple program handling input from user, mainly used to handles process management.
 * @version 0.1
 * @date 2022-06-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <pci.h>
#include <keyboard.h>
#include <screen.h>
#include <terminal.h>
#include <scheduler.h>
#include <pcb.h>
#include <kthreads.h>
#include <io.h>

#include <windowmanager.h>
#include <net/dns.h>
#include <net/icmp.h>
#include <fs/fs.h>

#include <serial.h>

#include <diskdev.h>

#include <gfx/gfxlib.h>


struct text_window w = {
	.x = 1,
	.y = 1,
	.height = SCREEN_HEIGHT-3,
	.width = SCREEN_WIDTH/1.5,
	.color = VGA_COLOR_LIGHT_BLUE,
	.visable = 1,
	.name = "TERMINAL",
	.state = {
		.column = 0,
		.row = SCREEN_HEIGHT-3,
		.color = VGA_COLOR_LIGHT_GREY
	}
	
};

#define SHELL_HEIGHT 275
#define SHELL_POSITION SHELL_HEIGHT-12
#define SHELL_MAX_SIZE SHELL_HEIGHT/8

static uint8_t shell_column = 0;
static char shell_buffer[SHELL_MAX_SIZE];
static uint8_t shell_buffer_length = 0;

static const char newline = '\n';
static const char backspace = '\b';

static char* shell_name = "Kernel >";

/*
 *	IMPLEMENTATIONS
 */
void shell_clear()
{
	gfx_draw_rectangle(0, SHELL_POSITION, SHELL_HEIGHT, 8, VESA8_COLOR_BLACK);
}

void reset_shell()
{
	shell_clear();
	memset(&shell_buffer, 0, SHELL_MAX_SIZE);
	shell_column = strlen(shell_name)+1;
	shell_buffer_length = 0;
	gfx_draw_text(0, SHELL_POSITION, shell_name, VESA8_COLOR_LIGHT_GREEN);
	shell_column += 1;

	//screen_set_cursor(shell_column, SHELL_POSITION);
	//shell_clear();
}

void exec_cmd()
{
	twritef("Kernel > %s", shell_buffer);

	if(strncmp("lspci", shell_buffer, strlen("lspci"))){
		list_pci_devices();
		return;
	}

	if(strncmp("ls", shell_buffer, strlen("ls"))){
		ls("");
		return;
	}

	if(strncmp("clear", shell_buffer, strlen("clear"))){
		scr_clear();
		return;
	}

	if(strncmp("queues", shell_buffer, strlen("queues"))){
		pcb_print_queues();
		return;
	}

	if(strncmp("stop", shell_buffer, strlen("stop"))){
		int id = atoi(shell_buffer+strlen("stop")+1);
		pcb_cleanup(id);
		return;
	}

	if(strncmp("block", shell_buffer, strlen("block"))){
		int id = atoi(shell_buffer+strlen("block")+1);
		pcb_set_blocked(id);
		return;
	}

	if(strncmp("unblock", shell_buffer, strlen("unblock"))){
		int id = atoi(shell_buffer+strlen("unblock")+1);
		pcb_set_running(id);
		return;
	}

	if(strncmp("dig", shell_buffer, strlen("dig"))){
		char* hostname = shell_buffer+strlen("dig")+1;
		hostname[strlen(hostname)-1] = 0;
		gethostname(hostname);
		return;
	}

	if(strncmp("cat", shell_buffer, strlen("cat"))){
		char* name = shell_buffer+strlen("cat")+1;
		inode_t inode = fs_open(name);

		char buf[512];
		fs_read(buf, inode);
		twritef("%s\n", buf);
		fs_close(inode);
		return;
	}

	if(strncmp("ping", shell_buffer, strlen("ping"))){
		char* hostname = shell_buffer+strlen("ping")+1;
		hostname[strlen(hostname)-1] = 0;
		ping(hostname);
		return;
	}

	if(strncmp("touch", shell_buffer, strlen("touch"))){
		char* filename = shell_buffer+strlen("touch")+1;
		filename[strlen(filename)-1] = 0;
		fs_create(filename);
		return;
	}

	if(strncmp("ps", shell_buffer, strlen("ps"))){
		print_pcb_status();
		return;
	}

	if(strncmp("fs", shell_buffer, strlen("fs"))){
		fs_stats();
		return;
	}

	if(strncmp("fdisk", shell_buffer, strlen("fdisk"))){
		print_dev_status();
		return;
	}

	if(strncmp("netinfo", shell_buffer, strlen("netinfo"))){
		networking_print_status();
		return;
	}

	if(strncmp("sync", shell_buffer, strlen("sync"))){
		sync();
		current_running->gfx_window->width = 400;
		current_running->gfx_window->height = 400;
		return;
	}

	if(strncmp("memmap", shell_buffer, strlen("memmap"))){
		pcb_memory_usage();
		memory_total_usage();
		return;
	}
	

	if(strncmp("exit", shell_buffer, strlen("exit"))){
		sync();
		dbgprintf("[SHUTDOWN] NETOS has shut down.\n");
		outportw(0x604, 0x2000);
		return;
	}

	if(strncmp("cd", shell_buffer, strlen("cd"))){
		char* name = shell_buffer+strlen("cd")+1;
		name[strlen(name)-1] = 0;
		chdir(name);
		return;
	}

	if(strncmp("mkdir", shell_buffer, strlen("mkdir"))){
		char* name = shell_buffer+strlen("mkdir")+1;
		name[strlen(name)-1] = 0;
		fs_mkdir(name);
		return;
	}

	if(strncmp("run", shell_buffer, strlen("run"))){
		char* name = shell_buffer+strlen("run")+1;
		name[strlen(name)-1] = 0;
		int pid = create_process(name);
		if(pid == 0)
			twritef("%s does not exist\n", name);

		return;
	}
	int r = start(shell_buffer);
	if(r == -1)
		twritef("Unknown command: %s\n", shell_buffer);
	else
		twriteln("Started process.");


	//twrite(shell_buffer);
}

/**
 * @brief Puts a character c into the shell line 
 * at correct position. Also detects enter.
 * 
 * @param c character to put to screen.
 */
void shell_put(char c)
{
	unsigned char uc = c;
	if(uc == newline)
	{
		shell_buffer[shell_buffer_length] = newline;
		shell_buffer_length++;
		exec_cmd();
		reset_shell();
		return;
	}

	if(uc == backspace)
	{
		if(shell_buffer_length < 1)
			return;
		shell_column -= 1;
		gfx_draw_rectangle(shell_column*8, SHELL_POSITION, 8, 8, VESA8_COLOR_BLACK);
		shell_buffer[shell_buffer_length] = 0;
		shell_buffer_length--;
		//screen_set_cursor(shell_column-1, SHELL_POSITION);
		return;
	}

	if(shell_column == SHELL_MAX_SIZE)
	{
		return;
	}
	gfx_draw_char(shell_column*8, SHELL_POSITION, uc, VESA8_COLOR_WHITE);
	shell_buffer[shell_buffer_length] = uc;
	shell_buffer_length++;
	screen_set_cursor(shell_column, SHELL_POSITION);
	shell_column++;
}

void shell_main()
{
	dbgprintf("Shell is running!\n");
	attach_window(&w);

	struct gfx_window* window = gfx_new_window(400, SHELL_HEIGHT);
	//gfx_draw_text(0, 0, "Terminal!", VESA8_COLOR_LIGHT_GREEN);
	terminal_fill();
	reset_shell();

	//gfx_draw_rectangle(0, 0, 300, 290, VESA8_COLOR_BLUE);
	//sleep(2);
	while(1)
	{
		char c = kb_get_char();
		if(c == -1)
			continue;
		shell_put(c);
	}
	
	exit();
}