#include "pos_system.h"

#define MAX_IPC_MESSAGES 100
#define IPC_BUFFER_SIZE 4096

typedef enum {
    MSG_NONE = 0,
    MSG_NEW_PRESCRIPTION,
    MSG_PRESCRIPTION_PROCESSED,
    MSG_PAYMENT_REQUEST,
    MSG_PAYMENT_COMPLETE,
    MSG_APPOINTMENT_SCHEDULED,
    MSG_PATIENT_CHECKED_IN,
    MSG_EQUIPMENT_REQUEST,
    MSG_EQUIPMENT_AVAILABLE,
    MSG_ALERT,
    MSG_DATA_SYNC,
    MSG_SYSTEM_SHUTDOWN
} message_type_t;

typedef enum {
    MODULE_KERNEL = 0,
    MODULE_DOCTOR,
    MODULE_MEDICATION,
    MODULE_CASHIER,
    MODULE_RECEPTION,
    MODULE_WAREHOUSE
} module_id_t;

typedef struct {
    uint32_t message_id;
    message_type_t type;
    module_id_t sender;
    module_id_t receiver;
    uint32_t timestamp;
    uint16_t data_size;
    uint8_t priority;
    uint8_t requires_ack;
    uint8_t acknowledged;
    char checksum[4];
    uint8_t data[256];
} ipc_message_t;

typedef struct {
    ipc_message_t messages[MAX_IPC_MESSAGES];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint8_t locked;
} ipc_queue_t;

ipc_queue_t message_queues[6]; // One for each module

void ipc_init() {
    for(int i = 0; i < 6; i++) {
        message_queues[i].head = 0;
        message_queues[i].tail = 0;
        message_queues[i].count = 0;
        message_queues[i].locked = 0;
    }
}

uint8_t ipc_send_message(module_id_t receiver, ipc_message_t* msg) {
    if(receiver >= 6) return 0;
    
    ipc_queue_t* queue = &message_queues[receiver];
    
    // Wait for lock
    while(queue->locked) {
        asm("pause");
    }
    queue->locked = 1;
    
    if(queue->count >= MAX_IPC_MESSAGES) {
        queue->locked = 0;
        return 0; // Queue full
    }
    
    // Copy message
    memcpy(&queue->messages[queue->tail], msg, sizeof(ipc_message_t));
    
    // Generate message ID
    static uint32_t next_message_id = 1;
    queue->messages[queue->tail].message_id = next_message_id++;
    
    // Calculate checksum
    calculate_message_checksum(&queue->messages[queue->tail]);
    
    queue->tail = (queue->tail + 1) % MAX_IPC_MESSAGES;
    queue->count++;
    
    queue->locked = 0;
    
    // Trigger interrupt to notify receiver
    send_ipc_notification(receiver);
    
    return 1;
}

uint8_t ipc_receive_message(module_id_t receiver, ipc_message_t* msg) {
    if(receiver >= 6) return 0;
    
    ipc_queue_t* queue = &message_queues[receiver];
    
    if(queue->count == 0) return 0;
    
    // Wait for lock
    while(queue->locked) {
        asm("pause");
    }
    queue->locked = 1;
    
    // Get message
    memcpy(msg, &queue->messages[queue->head], sizeof(ipc_message_t));
    
    queue->head = (queue->head + 1) % MAX_IPC_MESSAGES;
    queue->count--;
    
    queue->locked = 0;
    
    // Verify checksum
    if(!verify_message_checksum(msg)) {
        log_error("IPC checksum failed", "Message ID: %d", msg->message_id);
        return 0;
    }
    
    return 1;
}

uint8_t ipc_peek_message(module_id_t receiver, ipc_message_t* msg) {
    if(receiver >= 6) return 0;
    
    ipc_queue_t* queue = &message_queues[receiver];
    
    if(queue->count == 0) return 0;
    
    // Wait for lock
    while(queue->locked) {
        asm("pause");
    }
    queue->locked = 1;
    
    memcpy(msg, &queue->messages[queue->head], sizeof(ipc_message_t));
    
    queue->locked = 0;
    
    return 1;
}

void process_ipc_messages(module_id_t module) {
    ipc_message_t msg;
    
    while(ipc_receive_message(module, &msg)) {
        switch(msg.type) {
            case MSG_NEW_PRESCRIPTION:
                if(module == MODULE_MEDICATION) {
                    uint32_t prescription_id;
                    memcpy(&prescription_id, msg.data, sizeof(uint32_t));
                    process_prescription(prescription_id);
                }
                break;
                
            case MSG_PAYMENT_REQUEST:
                if(module == MODULE_CASHIER) {
                    uint32_t dispense_id;
                    memcpy(&dispense_id, msg.data, sizeof(uint32_t));
                    process_payment(dispense_id);
                }
                break;
                
            case MSG_EQUIPMENT_REQUEST:
                if(module == MODULE_WAREHOUSE) {
                    // Process equipment request
                    char equipment_code[16];
                    char department[32];
                    memcpy(equipment_code, msg.data, 16);
                    memcpy(department, msg.data + 16, 32);
                    
                    check_equipment_availability(equipment_code, department);
                }
                break;
                
            case MSG_ALERT:
                // Display alert on all modules
                display_alert((char*)msg.data);
                break;
                
            case MSG_SYSTEM_SHUTDOWN:
                // Prepare for shutdown
                prepare_shutdown();
                break;
        }
        
        // Send acknowledgment if required
        if(msg.requires_ack && !msg.acknowledged) {
            ipc_message_t ack;
            ack.type = MSG_NONE; // Special ack message
            ack.sender = module;
            ack.receiver = msg.sender;
            ack.requires_ack = 0;
            memcpy(ack.data, &msg.message_id, sizeof(uint32_t));
            ack.data_size = sizeof(uint32_t);
            
            ipc_send_message(msg.sender, &ack);
        }
    }
}

void calculate_message_checksum(ipc_message_t* msg) {
    // Simple checksum calculation
    uint32_t sum = 0;
    uint8_t* bytes = (uint8_t*)msg;
    
    for(uint32_t i = 0; i < offsetof(ipc_message_t, checksum); i++) {
        sum += bytes[i];
    }
    
    // Store in checksum field
    for(int i = 0; i < 4; i++) {
        msg->checksum[i] = (sum >> (8 * i)) & 0xFF;
    }
}

uint8_t verify_message_checksum(ipc_message_t* msg) {
    uint32_t sum = 0;
    uint8_t* bytes = (uint8_t*)msg;
    
    for(uint32_t i = 0; i < offsetof(ipc_message_t, checksum); i++) {
        sum += bytes[i];
    }
    
    // Verify checksum
    uint32_t stored_sum = 0;
    for(int i = 0; i < 4; i++) {
        stored_sum |= (msg->checksum[i] << (8 * i));
    }
    
    return sum == stored_sum;
}

void send_ipc_notification(module_id_t module) {
    // Send software interrupt to target module
    // This would be implemented with APIC or PIC in real system
    // For simplicity, we'll set a flag that modules check
    
    switch(module) {
        case MODULE_DOCTOR:
            doctor_ipc_flag = 1;
            break;
        case MODULE_MEDICATION:
            pharmacy_ipc_flag = 1;
            break;
        case MODULE_CASHIER:
            cashier_ipc_flag = 1;
            break;
        case MODULE_RECEPTION:
            reception_ipc_flag = 1;
            break;
        case MODULE_WAREHOUSE:
            warehouse_ipc_flag = 1;
            break;
    }
}
