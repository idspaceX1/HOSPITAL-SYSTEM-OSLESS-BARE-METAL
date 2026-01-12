#include "pos_system.h"

// VGA Text Mode Functions
void vga_clear_screen() {
    volatile char* video = (volatile char*)0xB8000;
    for(int i = 0; i < 80 * 25 * 2; i += 2) {
        video[i] = ' ';
        video[i+1] = 0x07; // Light grey on black
    }
    vga_cursor_x = 0;
    vga_cursor_y = 0;
    vga_update_cursor();
}

void vga_print_char(char c) {
    if(c == '\n') {
        vga_cursor_x = 0;
        vga_cursor_y++;
        if(vga_cursor_y >= 25) {
            vga_scroll();
            vga_cursor_y = 24;
        }
    } else {
        volatile char* video = (volatile char*)0xB8000;
        uint32_t offset = (vga_cursor_y * 80 + vga_cursor_x) * 2;
        video[offset] = c;
        video[offset + 1] = 0x07;
        
        vga_cursor_x++;
        if(vga_cursor_x >= 80) {
            vga_cursor_x = 0;
            vga_cursor_y++;
            if(vga_cursor_y >= 25) {
                vga_scroll();
                vga_cursor_y = 24;
            }
        }
    }
    vga_update_cursor();
}

void vga_print(const char* str) {
    while(*str) {
        vga_print_char(*str++);
    }
}

void vga_print_at(uint8_t x, uint8_t y, const char* str) {
    vga_cursor_x = x;
    vga_cursor_y = y;
    vga_print(str);
}

// Keyboard Functions
char keyboard_read_char() {
    while(1) {
        if(keyboard_buffer_read != keyboard_buffer_write) {
            char c = keyboard_buffer[keyboard_buffer_read];
            keyboard_buffer_read = (keyboard_buffer_read + 1) % KEYBOARD_BUFFER_SIZE;
            return c;
        }
        asm("hlt");
    }
}

char* keyboard_read_line(char* buffer, uint32_t max_len) {
    uint32_t idx = 0;
    
    while(1) {
        char c = keyboard_read_char();
        
        if(c == '\n' || c == '\r') {
            buffer[idx] = '\0';
            vga_print_char('\n');
            return buffer;
        } else if(c == 8 || c == 127) { // Backspace
            if(idx > 0) {
                idx--;
                vga_cursor_x--;
                vga_print_char(' ');
                vga_cursor_x--;
                vga_update_cursor();
            }
        } else if(c >= 32 && c < 127 && idx < max_len - 1) {
            buffer[idx++] = c;
            vga_print_char(c);
        }
    }
}

// Date/Time Functions
uint32_t get_cmos_date() {
    outb(CMOS_ADDRESS, 0x00); // Seconds
    uint8_t seconds = bcd_to_binary(inb(CMOS_DATA));
    
    outb(CMOS_ADDRESS, 0x02); // Minutes
    uint8_t minutes = bcd_to_binary(inb(CMOS_DATA));
    
    outb(CMOS_ADDRESS, 0x04); // Hours
    uint8_t hours = bcd_to_binary(inb(CMOS_DATA));
    
    outb(CMOS_ADDRESS, 0x07); // Day of month
    uint8_t day = bcd_to_binary(inb(CMOS_DATA));
    
    outb(CMOS_ADDRESS, 0x08); // Month
    uint8_t month = bcd_to_binary(inb(CMOS_DATA));
    
    outb(CMOS_ADDRESS, 0x09); // Year
    uint8_t year = bcd_to_binary(inb(CMOS_DATA));
    
    // Convert to UNIX timestamp (simplified)
    uint32_t timestamp = 0;
    timestamp += (year - 70) * 31536000; // Years since 1970
    timestamp += (month - 1) * 2592000;  // Months
    timestamp += (day - 1) * 86400;      // Days
    timestamp += hours * 3600;           // Hours
    timestamp += minutes * 60;           // Minutes
    timestamp += seconds;                // Seconds
    
    return timestamp;
}

void format_date(uint32_t timestamp, char* buffer) {
    // Convert timestamp to date string (YYYY-MM-DD)
    uint32_t days = timestamp / 86400;
    uint32_t years = 70 + days / 365; // Starting from 1970
    
    // Simplified - proper calculation would account for leap years
    uint32_t remaining_days = days % 365;
    uint32_t months = remaining_days / 30 + 1;
    uint32_t day_of_month = remaining_days % 30 + 1;
    
    sprintf(buffer, "%04d-%02d-%02d", 1900 + years, months, day_of_month);
}

// File System Functions (Simple FAT12-like)
void file_read(const char* filename, void* buffer, uint32_t size) {
    // Read from disk using BIOS or direct disk access
    // Simplified implementation
    
    // Convert filename to FAT12 format
    char fat_name[12];
    convert_to_fat_name(filename, fat_name);
    
    // Find file in directory
    uint32_t cluster = find_file_cluster(fat_name);
    
    if(cluster == 0) {
        // File not found
        return;
    }
    
    // Read cluster chain
    uint32_t bytes_read = 0;
    while(cluster < 0xFF8 && bytes_read < size) {
        uint32_t sector = cluster_to_sector(cluster);
        disk_read_sector(sector, buffer + bytes_read);
        bytes_read += 512;
        cluster = read_fat_entry(cluster);
    }
}

void file_write(const char* filename, void* data, uint32_t size) {
    // Write to disk
    char fat_name[12];
    convert_to_fat_name(filename, fat_name);
    
    // Find free clusters
    uint32_t clusters_needed = (size + 511) / 512;
    uint32_t* cluster_chain = allocate_clusters(clusters_needed);
    
    if(cluster_chain == NULL) {
        // No space
        return;
    }
    
    // Write directory entry
    create_directory_entry(fat_name, cluster_chain[0], size);
    
    // Write data
    for(uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t sector = cluster_to_sector(cluster_chain[i]);
        disk_write_sector(sector, data + (i * 512));
    }
    
    // Update FAT
    for(uint32_t i = 0; i < clusters_needed; i++) {
        if(i == clusters_needed - 1) {
            write_fat_entry(cluster_chain[i], 0xFFF);
        } else {
            write_fat_entry(cluster_chain[i], cluster_chain[i+1]);
        }
    }
}

// Mathematical Functions
float string_to_float(const char* str) {
    float result = 0.0;
    float fraction = 0.1;
    uint8_t decimal_found = 0;
    
    while(*str) {
        if(*str == '.') {
            decimal_found = 1;
        } else if(*str >= '0' && *str <= '9') {
            if(!decimal_found) {
                result = result * 10 + (*str - '0');
            } else {
                result += (*str - '0') * fraction;
                fraction *= 0.1;
            }
        }
        str++;
    }
    
    return result;
}

void float_to_string(float value, char* buffer, uint8_t decimals) {
    // Convert float to string with specified decimal places
    int32_t integer_part = (int32_t)value;
    float fractional_part = value - integer_part;
    
    // Convert integer part
    char int_buffer[20];
    int_to_str(integer_part, int_buffer);
    
    // Convert fractional part
    strcpy(buffer, int_buffer);
    strcat(buffer, ".");
    
    for(uint8_t i = 0; i < decimals; i++) {
        fractional_part *= 10;
        int digit = (int)fractional_part;
        buffer[strlen(buffer)] = '0' + digit;
        fractional_part -= digit;
    }
    buffer[strlen(buffer)] = '\0';
}

// CRC Calculation for Data Integrity
uint16_t calculate_crc16(const void* data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    const uint8_t* bytes = (const uint8_t*)data;
    
    for(uint32_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for(uint8_t j = 0; j < 8; j++) {
            if(crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

// Data Encryption (Simple XOR for demonstration)
void encrypt_data(void* data, uint32_t length, const char* key) {
    uint8_t* bytes = (uint8_t*)data;
    uint32_t key_len = strlen(key);
    
    for(uint32_t i = 0; i < length; i++) {
        bytes[i] ^= key[i % key_len];
    }
}

void decrypt_data(void* data, uint32_t length, const char* key) {
    // XOR encryption is symmetric
    encrypt_data(data, length, key);
}

// System Monitoring
void system_monitor() {
    // Display system status
    vga_clear_screen();
    vga_print_at(0, 0, "=== SYSTEM MONITOR ===");
    
    // Memory usage
    char mem_buf[40];
    sprintf(mem_buf, "Memory: %d/%d KB used", 
            memory_manager.used_memory / 1024,
            memory_manager.total_memory / 1024);
    vga_print_at(0, 2, mem_buf);
    
    // Task status
    vga_print_at(0, 4, "=== TASKS ===");
    for(int i = 0; i < MAX_TASKS; i++) {
        if(task_table[i].state != TASK_TERMINATED) {
            char task_buf[50];
            sprintf(task_buf, "%s: %s (CPU: %d)", 
                    task_table[i].name,
                    task_state_str(task_table[i].state),
                    task_table[i].cpu_time);
            vga_print_at(0, 5 + i, task_buf);
        }
    }
    
    // Interrupt counts
    vga_print_at(40, 4, "=== INTERRUPTS ===");
    for(int i = 0; i < 10; i++) {
        char int_buf[30];
        sprintf(int_buf, "INT %02d: %d", 
                i, interrupt_manager.interrupt_counters[i]);
        vga_print_at(40, 5 + i, int_buf);
    }
    
    // System uptime
    char uptime_buf[30];
    sprintf(uptime_buf, "Uptime: %d seconds", 
            system_status.uptime_seconds);
    vga_print_at(0, 20, uptime_buf);
    
    // Transactions
    char trans_buf[30];
    sprintf(trans_buf, "Transactions: %d", 
            system_status.transaction_counter);
    vga_print_at(0, 21, trans_buf);
    
    // Errors
    char error_buf[30];
    sprintf(error_buf, "Errors: %d", 
            system_status.error_count);
    vga_print_at(0, 22, error_buf);
    
    vga_print_at(0, 24, "Press any key to continue...");
    keyboard_read_char();
}
