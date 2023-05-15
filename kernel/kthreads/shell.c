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
#include <terminal.h>
#include <scheduler.h>
#include <pcb.h>
#include <rtc.h>
#include <kthreads.h>
#include <ksyms.h>
#include <arch/io.h>

#include <windowmanager.h>
#include <net/dns.h>
#include <net/icmp.h>
#include <fs/fs.h>

#include <serial.h>

#include <diskdev.h>

#include <gfx/gfxlib.h>
#include <gfx/theme.h>
#include <gfx/composition.h>
#include <gfx/events.h>

#define SHELL_HEIGHT 275 /* 275 */
#define SHELL_WIDTH 400 /* 400 */
#define SHELL_POSITION SHELL_HEIGHT-12
#define SHELL_MAX_SIZE SHELL_WIDTH/2

static uint8_t shell_column = 0;
static char shell_buffer[SHELL_MAX_SIZE];
static uint8_t shell_buffer_length = 0;

static const char newline = '\n';
static const char backspace = '\b';

static char* shell_name = "Kernel >";

static struct terminal term  = {
	.head = 0,
	.tail = 0,
	.lines = 0
};

/*
 *	IMPLEMENTATIONS
 */
void shell_clear()
{
	struct gfx_theme* theme = kernel_gfx_current_theme();
	kernel_gfx_draw_rectangle(0, SHELL_POSITION, gfx_get_window_height(), 8, theme->terminal.background);
}

void reset_shell()
{
	shell_clear();
	memset(&shell_buffer, 0, SHELL_MAX_SIZE);
	shell_column = strlen(shell_name)+1;
	shell_buffer_length = 0;
	kernel_gfx_draw_text(0, SHELL_POSITION, shell_name, COLOR_VGA_MISC);
	shell_column += 1;
}

void ps()
{
	int ret;
	int line = 0;
	twritef("  PID  STACK       TYPE     STATE     NAME\n");
	for (int i = 0; i < MAX_NUM_OF_PCBS; i++){
		struct pcb_info info;
		ret = pcb_get_info(i, &info);
		if(ret < 0) continue;
		twritef("   %d   0x%s%x  %s  %s  %s\n", info.pid, info.is_process ? "" : "00", info.stack, info.is_process ? "process" : "kthread", pcb_status[info.state], info.name);
	}
}
EXPORT_KSYMBOL(ps);

void run(int argc, char* argv[])
{
	char* optarg = NULL;
    int opt = 0;
	while ((opt = getopt(argc, argv, "hc:", &optarg)) != -1) {
		dbgprintf("%c\n", opt);
        switch (opt) {
            case 'h':
                twritef("run [hn]\n  n - name\n  h - help\n  example: run -c /bin/clock\n");
                return;
			case 'c':
				dbgprintf("c flag set\n");
				{
					int r = start(optarg);
					if(r >= 0){
						twritef("Kernel thread started\n");
						return;
					}

					int pid = pcb_create_process(optarg, 0, NULL);
					if(pid < 0)
						twritef("%s does not exist\n", optarg);
				}
				return;
            case '?':
                twritef("Invalid option\n");
				return;
            case ':':
                twritef("Missing option argument\n");
				return;
            default:
                twritef("Unknown option %c\n", opt);
				return;
        }
	}
	
	twritef("Missing option argument: -h for help\n");
    return;
}
EXPORT_KSYMBOL(run);

/**
 * @brief cmd: command [opts] [args]
 * 
 * command:
 * 	resolved from ksyms
 * 
 * opts:
 *  -t run as a thread (long)
 *  -w run as a worker (short)
 * 
 * args: 
 * 	arguments passed to command
 * 
 */

void ths()
{
	dbgprintf("%d\n", 0x1337);
	int total_themes = gfx_total_themes();
	for (int i = 0; i < total_themes; i++){
		twritef("%d) %s\n", i, kernel_gfx_get_theme(i)->name);
	}
}
EXPORT_KSYMBOL(ths);

void dig(int argc, char* argv[])
{
	int ret = gethostname(argv[1]);
	twritef("%s IN (A) %i\n", argv[1], ret);
}
EXPORT_KSYMBOL(dig);

void th(int argc, char* argv[])
{
	int id = atoi(argv[1]);
	kernel_gfx_set_theme(id);
}
EXPORT_KSYMBOL(th);

void cd(int argc, char* argv[])
{
	current_running->current_directory = chdir(argv[1]);
}
EXPORT_KSYMBOL(cd);

void cat(int argc, char* argv[])
{
	inode_t inode = fs_open(argv[1]);

	char buf[512];
	fs_read(inode, buf, 512);
	twritef("%s\n", buf);
	fs_close(inode);
	return;
}
EXPORT_KSYMBOL(cat);

void ls()
{
	listdir();
}
EXPORT_KSYMBOL(ls);

void help()
{
	twritef("Help:\n  run - Run a new thread / process.\n  th - Change theme\n  ths - List themes\n");
}
EXPORT_KSYMBOL(help);


char* welcome = "\n\
       _..--=--..._\n\
    .-'            '-.  .-.\n\
   /.'              '.\\/  /\n\
  |=-                -=| (  NETOS\n\
   \\'.              .'/\\  \\\n\
    '-.,_____ _____.-'  '-'\n\
         [_____]=8\n";

char** argv = NULL;

void exec_cmd()
{
	for (int i = 0; i < 5; i++) memset(argv[i], 0, 100);
	int argc = parse_arguments(shell_buffer, argv);
	if(argc == 0) return;

	void (*ptr)(int argc, char* argv[]) = (void (*)(int argc, char* argv[])) ksyms_resolve_symbol(argv[0]);
	if(ptr == NULL){
		twritef("Unknown command\n");
		return;
	}

	twritef("Kernel > %s", shell_buffer);
	ptr(argc, argv);
	twritef("\n");
	gfx_commit();

	return;

    //int numArgs = shell_parse_command(str, command, args);

	if(strncmp("ls", shell_buffer, strlen("ls"))){
		ls("");
		gfx_commit();
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
		int ret = gethostname(hostname);
		twritef("%s IN (A) %i\n", hostname, ret);
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

	if(strncmp("sync", shell_buffer, strlen("sync"))){
		sync();
		current_running->gfx_window->width = 400;
		current_running->gfx_window->height = 400;
		return;
	}

	if(strncmp("exit", shell_buffer, strlen("exit"))){
		sync();
		dbgprintf("[SHUTDOWN] NETOS has shut down.\n");
		outportw(0x604, 0x2000);
		return;
	}

	if(strncmp("mkdir", shell_buffer, strlen("mkdir"))){
		char* name = shell_buffer+strlen("mkdir")+1;
		name[strlen(name)-1] = 0;
		fs_mkdir(name, current_running->current_directory);
		return;
	}

	gfx_commit();
	//twrite(shell_buffer);
}

/**
 * @brief Puts a character c into the shell line 
 * at correct position. Also detects enter.
 * 
 * @param c character to put to screen.
 */
void shell_put(unsigned char c)
{
	unsigned char uc = c;
	if(uc == newline)
	{
		shell_buffer[shell_buffer_length] = newline;
		shell_buffer_length++;
		exec_cmd();
		terminal_commit();
		reset_shell();
		
		return;
	}

	if(uc == backspace)
	{
		if(shell_buffer_length < 1)
			return;
		shell_column -= 1;
		kernel_gfx_draw_rectangle(shell_column*8, SHELL_POSITION, 8, 8, COLOR_VGA_BG);
		gfx_commit();
		shell_buffer[shell_buffer_length] = 0;
		shell_buffer_length--;
		return;
	}

	if(shell_column == SHELL_MAX_SIZE)
	{
		return;
	}
	kernel_gfx_draw_char(shell_column*8, SHELL_POSITION, uc, COLOR_VGA_FG);
	gfx_commit();
	shell_buffer[shell_buffer_length] = uc;
	shell_buffer_length++;
	shell_column++;
}

#include <gfx/api.h>

int c_test = 0;
void shell()
{
	dbgprintf("shell is running!\n");

	memset(term.textbuffer, 0, TERMINAL_BUFFER_SIZE);
	struct gfx_window* window = gfx_new_window(SHELL_WIDTH, SHELL_HEIGHT);
	
	dbgprintf("shell: window 0x%x\n", window);
	kernel_gfx_draw_rectangle(0,0, gfx_get_window_width(), gfx_get_window_height(), COLOR_VGA_BG);
	//gfx_set_fullscreen(window);	

	argv = (char**)kalloc(5 * sizeof(char*));
	for (int i = 0; i < 5; i++) {
		argv[i] = (char*)kalloc(100);
		memset(argv[i], 0, 100);
	}

	terminal_attach(&term);
	//kernel_gfx_draw_text(0, 0, "Terminal!", VESA8_COLOR_BOX_LIGHT_GREEN);

	struct mem_info minfo;
    get_mem_info(&minfo);

	//gfx_set_fullscreen(window);
	//sleep(2);
	twritef("_.--*/ \\*--._\nWelcome ADMIN!\n");
	twritef("%s\n", welcome);
	twritef("Memory: %d/%d\n", minfo.kernel.used+minfo.permanent.used, minfo.kernel.total+minfo.permanent.total);
	help();
	twriteln("");
	terminal_commit(current_running->term);

	reset_shell();
	while(1)
	{
		struct gfx_event event;
		gfx_event_loop(&event);

		switch (event.event)
		{
		case GFX_EVENT_KEYBOARD:
			if(event.data == -1)
				break;
			shell_put(event.data);
			c_test++;
		default:
			break;
		}
	}
	
	kernel_exit();
}
EXPORT_KTHREAD(shell);