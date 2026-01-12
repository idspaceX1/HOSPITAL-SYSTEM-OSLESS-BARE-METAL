#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Hardware Ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43
#define KEYBOARD_DATA 0x60
#define KEYBOARD_STATUS 0x64
#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71
#define VGA_CTRL 0x3D4
#define VGA_DATA 0x3D5
#define LPT1_DATA 0x378
#define LPT1_STATUS 0x379
#define LPT1_CONTROL 0x37A

// Memory Management
#define MEMORY_POOL_SIZE 0x100000
#define MAX_MEMORY_BLOCKS 1024

typedef struct {
    uint32_t start;
    uint32_t size;
    bool allocated;
    char owner[32];
} memory_block_t;

typedef struct {
    memory_block_t blocks[MAX_MEMORY_BLOCKS];
    uint32_t total_blocks;
    uint32_t total_memory;
    uint32_t used_memory;
} memory_manager_t;

// Interrupt Handling
#define MAX_INTERRUPTS 256

typedef struct {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t ss;
} interrupt_frame_t;

typedef void (*isr_handler_t)(interrupt_frame_t*);

typedef struct {
    isr_handler_t handlers[MAX_INTERRUPTS];
    uint32_t interrupt_counters[MAX_INTERRUPTS];
} interrupt_manager_t;

// Task Management
#define MAX_TASKS 16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef struct {
    uint32_t task_id;
    char name[32];
    task_state_t state;
    uint32_t* stack_pointer;
    uint32_t* stack_base;
    uint32_t priority;
    uint32_t time_slice;
    uint32_t cpu_time;
    void (*entry_point)(void*);
    void* parameter;
    uint32_t registers[8]; // EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP
} task_t;

// System Tables
typedef struct {
    uint32_t system_time;
    uint32_t uptime_seconds;
    uint8_t hardware_initialized;
    uint8_t modules_loaded;
    uint32_t transaction_counter;
    uint32_t user_counter;
    uint32_t error_count;
    char serial_number[20];
    char system_version[16];
} system_status_t;

// Global System Variables
memory_manager_t memory_manager;
interrupt_manager_t interrupt_manager;
task_t task_table[MAX_TASKS];
uint32_t current_task = 0;
system_status_t system_status;

// Hardware Initialization
void init_pic() {
    // Initialize Primary PIC
    outb(PIC1_COMMAND, 0x11); // ICW1: Initialize, edge-triggered
    outb(PIC1_DATA, 0x20);    // ICW2: Interrupt vector offset
    outb(PIC1_DATA, 0x04);    // ICW3: Master/slave wiring
    outb(PIC1_DATA, 0x01);    // ICW4: 8086 mode
    
    // Initialize Secondary PIC
    outb(PIC2_COMMAND, 0x11);
    outb(PIC2_DATA, 0x28);
    outb(PIC2_DATA, 0x02);
    outb(PIC2_DATA, 0x01);
    
    // Mask all interrupts
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void init_pit() {
    // Set PIT to 100Hz (10ms ticks)
    uint16_t divisor = 1193180 / 100;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

void init_keyboard() {
    // Enable keyboard interrupts
    outb(PIC1_DATA, inb(PIC1_DATA) & ~0x02);
    // Self-test
    outb(KEYBOARD_STATUS, 0xAA);
    while((inb(KEYBOARD_STATUS) & 0x02) != 0);
    // Enable scanning
    outb(KEYBOARD_DATA, 0xF4);
}

// Memory Management
void init_memory_manager() {
    memory_manager.total_blocks = 0;
    memory_manager.total_memory = MEMORY_POOL_SIZE;
    memory_manager.used_memory = 0;
    
    // Initialize first block covering all available memory
    memory_manager.blocks[0].start = 0x100000;
    memory_manager.blocks[0].size = MEMORY_POOL_SIZE;
    memory_manager.blocks[0].allocated = false;
    strcpy(memory_manager.blocks[0].owner, "SYSTEM");
    memory_manager.total_blocks = 1;
}

void* kmalloc(uint32_t size, const char* owner) {
    // Find first fit block
    for(uint32_t i = 0; i < memory_manager.total_blocks; i++) {
        if(!memory_manager.blocks[i].allocated && 
           memory_manager.blocks[i].size >= size) {
            
            // Split block if larger than needed
            if(memory_manager.blocks[i].size > size + sizeof(memory_block_t)) {
                // Create new free block
                uint32_t new_block_index = memory_manager.total_blocks++;
                memory_manager.blocks[new_block_index].start = 
                    memory_manager.blocks[i].start + size;
                memory_manager.blocks[new_block_index].size = 
                    memory_manager.blocks[i].size - size;
                memory_manager.blocks[new_block_index].allocated = false;
                strcpy(memory_manager.blocks[new_block_index].owner, "FREE");
                
                // Resize current block
                memory_manager.blocks[i].size = size;
            }
            
            memory_manager.blocks[i].allocated = true;
            strcpy(memory_manager.blocks[i].owner, owner);
            memory_manager.used_memory += size;
            
            return (void*)memory_manager.blocks[i].start;
        }
    }
    return NULL;
}

void kfree(void* ptr) {
    for(uint32_t i = 0; i < memory_manager.total_blocks; i++) {
        if(memory_manager.blocks[i].start == (uint32_t)ptr) {
            memory_manager.blocks[i].allocated = false;
            memory_manager.used_memory -= memory_manager.blocks[i].size;
            strcpy(memory_manager.blocks[i].owner, "FREE");
            
            // Merge with adjacent free blocks
            // Implementation omitted for brevity
            
            return;
        }
    }
}

// Task Scheduler
void init_task_manager() {
    for(int i = 0; i < MAX_TASKS; i++) {
        task_table[i].state = TASK_TERMINATED;
        task_table[i].stack_base = kmalloc(TASK_STACK_SIZE, "TASK_STACK");
    }
    
    // Create idle task
    create_task("IDLE", idle_task, NULL, 0);
}

uint32_t create_task(const char* name, void (*entry)(void*), void* param, uint32_t priority) {
    for(int i = 0; i < MAX_TASKS; i++) {
        if(task_table[i].state == TASK_TERMINATED) {
            task_table[i].task_id = i;
            strcpy(task_table[i].name, name);
            task_table[i].state = TASK_READY;
            task_table[i].priority = priority;
            task_table[i].time_slice = 100; // 1 second at 100Hz
            task_table[i].cpu_time = 0;
            task_table[i].entry_point = entry;
            task_table[i].parameter = param;
            
            // Initialize stack
            uint32_t* stack = task_table[i].stack_base + TASK_STACK_SIZE / 4;
            *(--stack) = 0x202; // EFLAGS
            *(--stack) = 0x8;   // CS
            *(--stack) = (uint32_t)entry; // EIP
            *(--stack) = 0; // EAX
            *(--stack) = 0; // EBX
            *(--stack) = 0; // ECX
            *(--stack) = 0; // EDX
            *(--stack) = 0; // ESI
            *(--stack) = 0; // EDI
            *(--stack) = 0; // EBP
            task_table[i].stack_pointer = stack;
            
            return i;
        }
    }
    return 0xFFFFFFFF;
}

void schedule() {
    // Round-robin scheduler with priority
    uint32_t next_task = current_task;
    
    do {
        next_task = (next_task + 1) % MAX_TASKS;
        if(task_table[next_task].state == TASK_READY) {
            switch_task(next_task);
            return;
        }
    } while(next_task != current_task);
}

// Interrupt Handlers
void isr_timer(interrupt_frame_t* frame) {
    system_status.system_time++;
    system_status.uptime_seconds = system_status.system_time / 100;
    
    // Update task time slices
    if(task_table[current_task].state == TASK_RUNNING) {
        if(--task_table[current_task].time_slice == 0) {
            task_table[current_task].state = TASK_READY;
            task_table[current_task].time_slice = 100;
            schedule();
        }
        task_table[current_task].cpu_time++;
    }
    
    // Send EOI to PIC
    outb(PIC1_COMMAND, 0x20);
}

void isr_keyboard(interrupt_frame_t* frame) {
    uint8_t scancode = inb(KEYBOARD_DATA);
    
    // Process keyboard input
    // Forward to active module
    
    outb(PIC1_COMMAND, 0x20);
}

// System Calls
#define SYSCALL_PRINT   0
#define SYSCALL_READ    1
#define SYSCALL_MALLOC  2
#define SYSCALL_FREE    3
#define SYSCALL_TIME    4
#define SYSCALL_IOCTL   5

void syscall_handler(interrupt_frame_t* frame) {
    uint32_t syscall_num = frame->eax;
    uint32_t param1 = frame->ebx;
    uint32_t param2 = frame->ecx;
    uint32_t param3 = frame->edx;
    
    switch(syscall_num) {
        case SYSCALL_PRINT:
            vga_print((char*)param1);
            break;
        case SYSCALL_READ:
            frame->eax = keyboard_read();
            break;
        case SYSCALL_MALLOC:
            frame->eax = (uint32_t)kmalloc(param1, (char*)param2);
            break;
        case SYSCALL_FREE:
            kfree((void*)param1);
            break;
        case SYSCALL_TIME:
            frame->eax = system_status.system_time;
            break;
    }
}

// Main Kernel Entry Point
void kernel_main() {
    // Initialize hardware
    init_pic();
    init_pit();
    init_keyboard();
    
    // Initialize managers
    init_memory_manager();
    init_task_manager();
    
    // Load modules
    load_module("DOCTOR.BIN", 0x20000);
    load_module("MEDICATION.BIN", 0x30000);
    load_module("CASHIER.BIN", 0x40000);
    load_module("RECEPTION.BIN", 0x50000);
    load_module("WAREHOUSE.BIN", 0x60000);
    
    // Start scheduler
    enable_interrupts();
    
    // System ready
    vga_print("Hospital POS System v1.0 Ready\n");
    
    // Never return
    while(1) {
        asm("hlt");
    }
}
