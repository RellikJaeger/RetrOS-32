#ifndef PCB_H
#define PCB_H

struct pcb;

#include <util.h>
#include <sync.h>
#include <gfx/window.h>
#include <memory.h>
#include <fs/inode.h>
#include <errors.h>

#define MAX_NUM_OF_PCBS 64
#define PCB_MAX_NAME_LENGTH 25

/* TODO: Move to new file */
typedef enum pcb_states {
    STOPPED,
    RUNNING,
    PCB_NEW,
    BLOCKED,
    SLEEPING,
    ZOMBIE,
    CLEANING
} pcb_state_t;

typedef enum pcb_flags {
    PCB_FLAG_KERNEL = 1 << 1,
} pcb_flag_t;

struct pcb_state {
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t eip;
    uint32_t eflags;    /* Field for the EFLAGS register */
    uint8_t fpu_state[108];
};

struct pcb {
    struct pcb_state ctx;
    int args;
    char** argv;
    uint32_t ds;            /* Data segment selector */
    uint32_t cs;            /* Code segment selector */
    uint32_t kesp;
    uint32_t kebp;
    uint8_t is_process;
    /* DO NOT NOT CHANGE ABOVE.*/
    char name[PCB_MAX_NAME_LENGTH];
    pcb_state_t state;
    int16_t pid;
    uint16_t sleep;
    uint32_t stackptr;
    uint32_t* page_dir;
    uint32_t data_size;

    /* stats */
    int kallocs;
    int preempts;
    int yields;
    uint32_t blocked_count;

    struct window* gfx_window;
    struct terminal* term;

    inode_t current_directory;
    

    uintptr_t thread_eip;
    
    struct virtual_allocations* allocations;
    int used_memory;

    struct pcb* parent;
    struct pcb *next;
    struct pcb *prev;
}__attribute__((__packed__));

struct pcb_info {
    uint8_t pid;
    uint8_t state;
    uint32_t stack;
    uint32_t used_memory;
    uint8_t is_process;
    float usage;
    char name[PCB_MAX_NAME_LENGTH];
};

extern const char* pcb_status[];
extern struct pcb* current_running;

/* Forward declaration */
struct pcb_queue;

/**
 * @brief This defines a set of operations for a PCB queue.
 * The pcb_queue_operations structure defines a set of operations that can be performed
 * on a PCB queue. These operations include pushing and removing PCBs from the queue,
 * as well as getting the next PCB to be executed.
 */
struct pcb_queue_operations {
	error_t (*push)(struct pcb_queue* queue, struct pcb* pcb);
	error_t (*add)(struct pcb_queue* queue, struct pcb* pcb);
	void (*remove)(struct pcb_queue* queue, struct pcb* pcb);
	struct pcb* (*pop)(struct pcb_queue* queue);
    struct pcb* (*peek)(struct pcb_queue* queue);
};

/**
 * @brief This defines a PCB queue.
 * The pcb_queue structure defines a PCB queue, which contains a set of operations defined
 * by the pcb_queue_operations structure. It also contains a pointer to the head of the queue,
 * a spinlock to protect access to the queue, and a count of the total number of PCBs in the queue.
 */
struct pcb_queue {
	struct pcb_queue_operations* ops;
	struct pcb* _list;
	spinlock_t spinlock;
	int total;
};

void init_pcbs();
void pcb_start();
void start_pcb(struct pcb* pcb);
error_t pcb_create_kthread( void (*entry)(), char* name);
error_t pcb_create_process(char* program, int args, char** argv, pcb_flag_t flags);

void pcb_set_running(int pid);
void pcb_kill(int pid);

void pcb_dbg_print(struct pcb* pcb);

void pcb_queue_remove_running(struct pcb* pcb);
void pcb_queue_push_running(struct pcb* pcb);

int pcb_cleanup_routine(int pid);

error_t pcb_get_info(int pid, struct pcb_info* info);

struct pcb* pcb_get_new_running();
struct pcb_queue* pcb_new_queue();

/* functions in entry.s */
void _start_pcb(struct pcb* pcb);
void context_switch_entry();

struct pcb* pcb_get_by_pid(int pid);

/* declaration in helpers.s */
void pcb_restore_context();
void pcb_save_context();

void idletask();
void dummytask();
void Genesis();


#endif