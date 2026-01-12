#ifndef POS_SYSTEM_H
#define POS_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

// Basic Type Definitions
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// Hardware I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait() {
    outb(0x80, 0);
}

// Memory Operations
void* memcpy(void* dest, const void* src, uint32_t n);
void* memset(void* s, int c, uint32_t n);
int memcmp(const void* s1, const void* s2, uint32_t n);

// String Operations
uint32_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
int strcmp(const char* s1, const char* s2);
char* strstr(const char* haystack, const char* needle);

// VGA Text Mode
extern uint8_t vga_cursor_x;
extern uint8_t vga_cursor_y;
void vga_clear_screen();
void vga_print_char(char c);
void vga_print(const char* str);
void vga_print_at(uint8_t x, uint8_t y, const char* str);
void vga_update_cursor();
void vga_scroll();

// Keyboard
#define KEYBOARD_BUFFER_SIZE 256
extern char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
extern uint32_t keyboard_buffer_read;
extern uint32_t keyboard_buffer_write;
void keyboard_init();
char keyboard_read_char();
char* keyboard_read_line(char* buffer, uint32_t max_len);

// Date/Time
uint32_t get_system_time();
uint32_t get_system_date();
void format_date(uint32_t timestamp, char* buffer);
void format_time(uint32_t timestamp, char* buffer);
uint32_t read_date();
uint32_t add_days(uint32_t date, uint16_t days);
uint8_t calculate_age(uint32_t birth_date);

// File System
void file_read(const char* filename, void* buffer, uint32_t size);
void file_write(const char* filename, void* data, uint32_t size);
void file_delete(const char* filename);
uint32_t file_size(const char* filename);
uint8_t file_exists(const char* filename);

// Database Functions
void load_patient_database();
void save_patient_database();
void load_medication_database();
void save_medication_database();
void load_transaction_database();
void save_transaction_database();

// Utility Functions
int atoi(const char* str);
char* itoa(int value, char* str, int base);
float string_to_float(const char* str);
void float_to_string(float value, char* buffer, uint8_t decimals);
uint16_t calculate_crc16(const void* data, uint32_t length);
void encrypt_data(void* data, uint32_t length, const char* key);
void decrypt_data(void* data, uint32_t length, const char* key);

// System Functions
void delay(uint32_t milliseconds);
void beep(uint32_t frequency, uint32_t duration);
void system_shutdown();
void system_restart();
void log_activity(const char* category, const char* message, ...);
void log_error(const char* category, const char* message, ...);

// Module Entry Points
void doctor_main();
void medication_main();
void cashier_main();
void reception_main();
void warehouse_main();

#endif // POS_SYSTEM_H
