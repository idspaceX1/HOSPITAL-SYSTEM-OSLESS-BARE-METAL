#include "pos_system.h"

#define MAX_EQUIPMENT_TYPES 500
#define MAX_EQUIPMENT_ITEMS 5000
#define MAX_MAINTENANCE_RECORDS 2000
#define MAX_SUPPLIERS 100

typedef struct {
    char equipment_code[16];
    char name[64];
    char category[32]; // Diagnostic, Therapeutic, Surgical, Monitoring
    char manufacturer[64];
    char model[64];
    char serial_number_format[20];
    uint16_t expected_life_years;
    float purchase_price;
    float current_value;
    float depreciation_rate;
    uint8_t requires_calibration;
    uint16_t calibration_interval_days;
    uint8_t requires_maintenance;
    uint16_t maintenance_interval_days;
    char storage_requirements[64];
    uint16_t min_quantity;
    uint16_t max_quantity;
} equipment_type_t;

typedef struct {
    uint32_t item_id;
    char equipment_code[16];
    char serial_number[30];
    char asset_tag[20];
    uint32_t purchase_date;
    float purchase_price;
    char supplier[64];
    char purchase_order[20];
    char location[32]; // Room number, shelf, etc.
    char status[16]; // Available, In-use, Maintenance, Calibration, Retired
    uint32_t last_maintenance;
    uint32_t next_maintenance;
    uint32_t last_calibration;
    uint32_t next_calibration;
    uint8_t calibration_due;
    uint8_t maintenance_due;
    float current_value;
    uint32_t usage_hours;
} equipment_item_t;

typedef struct {
    uint32_t maintenance_id;
    uint32_t item_id;
    uint32_t date;
    char type[16]; // Preventive, Corrective, Calibration
    char description[128];
    char technician[32];
    float cost;
    char parts_used[256];
    uint32_t next_maintenance_date;
    char status[16]; // Scheduled, In-progress, Completed
} maintenance_record_t;

typedef struct {
    uint32_t transaction_id;
    uint32_t date;
    char transaction_type[16]; // Check-out, Check-in, Transfer, Retirement
    uint32_t item_id;
    char from_location[32];
    char to_location[32];
    char user[32];
    char department[32];
    char purpose[64];
    uint32_t expected_return;
    uint32_t actual_return;
    char condition[64];
} equipment_transaction_t;

// Databases
equipment_type_t equipment_type_db[MAX_EQUIPMENT_TYPES];
equipment_item_t equipment_item_db[MAX_EQUIPMENT_ITEMS];
maintenance_record_t maintenance_db[MAX_MAINTENANCE_RECORDS];
equipment_transaction_t transaction_db[MAX_EQUIPMENT_ITEMS * 10]; // 10 transactions per item avg

void equipment_checkout() {
    clear_screen();
    print_header("EQUIPMENT CHECK-OUT");
    
    // Search equipment
    print("Search equipment (code/name/serial): ");
    char search[32];
    read_input(search, 32);
    
    // Find available equipment
    uint32_t results[20];
    uint32_t result_count = 0;
    
    for(uint32_t i = 0; i < MAX_EQUIPMENT_ITEMS; i++) {
        if(strcmp(equipment_item_db[i].status, "AVAILABLE") == 0) {
            // Check if matches search
            if(strstr(equipment_item_db[i].equipment_code, search) != NULL ||
               strstr(equipment_item_db[i].serial_number, search) != NULL) {
                
                equipment_type_t* type = 
                    find_equipment_type(equipment_item_db[i].equipment_code);
                
                if(type && strstr(type->name, search) != NULL) {
                    results[result_count++] = i;
                }
            }
        }
    }
    
    if(result_count == 0) {
        print("No available equipment found.\n");
        wait_key();
        return;
    }
    
    // Display results
    print("\n%4s %-16s %-20s %-12s %s\n",
          "#", "Code", "Equipment", "Serial", "Location");
    print("------------------------------------------------------------\n");
    
    for(uint32_t i = 0; i < result_count; i++) {
        uint32_t idx = results[i];
        equipment_type_t* type = 
            find_equipment_type(equipment_item_db[idx].equipment_code);
        
        print("%4d %-16s %-20s %-12s %s\n",
              i+1,
              equipment_item_db[idx].equipment_code,
              type->name,
              equipment_item_db[idx].serial_number,
              equipment_item_db[idx].location);
    }
    
    print("\nSelect equipment (0 to cancel): ");
    uint32_t selection = read_uint();
    
    if(selection == 0 || selection > result_count) return;
    
    uint32_t item_idx = results[selection - 1];
    equipment_item_t* item = &equipment_item_db[item_idx];
    
    // Get checkout details
    print("\n=== CHECK-OUT DETAILS ===\n");
    
    char user[32];
    char department[32];
    char purpose[64];
    uint32_t duration_days;
    
    print("User Name: ");
    read_input(user, 32);
    
    print("Department: ");
    read_input(department, 32);
    
    print("Purpose: ");
    read_input(purpose, 64);
    
    print("Duration (days): ");
    duration_days = read_uint();
    
    // Check maintenance status
    if(item->maintenance_due) {
        print("WARNING: Maintenance due!\n");
        print("Proceed anyway? (Y/N): ");
        char proceed = getchar();
        if(proceed != 'Y' && proceed != 'y') return;
    }
    
    if(item->calibration_due) {
        print("WARNING: Calibration due!\n");
        print("Proceed anyway? (Y/N): ");
        char proceed = getchar();
        if(proceed != 'Y' && proceed != 'y') return;
    }
    
    // Create transaction
    equipment_transaction_t* trans = find_empty_transaction_slot();
    trans->transaction_id = generate_transaction_id();
    trans->date = get_system_time();
    strcpy(trans->transaction_type, "CHECK-OUT");
    trans->item_id = item->item_id;
    strcpy(trans->from_location, item->location);
    strcpy(trans->to_location, department);
    strcpy(trans->user, user);
    strcpy(trans->department, department);
    strcpy(trans->purpose, purpose);
    trans->expected_return = add_days(get_system_time(), duration_days);
    strcpy(trans->condition, "GOOD");
    
    // Update equipment status
    strcpy(item->status, "IN-USE");
    strcpy(item->location, department);
    
    // Print checkout slip
    print_checkout_slip(trans);
    
    // Print barcode label
    print_equipment_label(item);
    
    log_activity("Equipment checked out",
                "Item: %s, Serial: %s, User: %s",
                item->equipment_code, item->serial_number, user);
    
    print("\nEquipment checked out successfully!\n");
    wait_key();
}

void equipment_checkin() {
    clear_screen();
    print_header("EQUIPMENT CHECK-IN");
    
    print("Scan equipment barcode or enter serial: ");
    char serial[30];
    read_input(serial, 30);
    
    // Find equipment
    equipment_item_t* item = find_equipment_by_serial(serial);
    if(item == NULL) {
        print("Equipment not found!\n");
        wait_key();
        return;
    }
    
    if(strcmp(item->status, "IN-USE") != 0) {
        print("Equipment status: %s\n", item->status);
        wait_key();
        return;
    }
    
    // Find open transaction
    equipment_transaction_t* trans = find_open_transaction(item->item_id);
    if(trans == NULL) {
        print("No open transaction found!\n");
        wait_key();
        return;
    }
    
    print("\n=== EQUIPMENT DETAILS ===\n");
    equipment_type_t* type = find_equipment_type(item->equipment_code);
    
    print("Equipment: %s\n", type->name);
    print("Serial: %s\n", item->serial_number);
    print("Checked out to: %s\n", trans->user);
    print("Department: %s\n", trans->department);
    print("Expected return: %s\n", format_date(trans->expected_return));
    
    // Check condition
    print("\nCondition check:\n");
    print("1. Good\n");
    print("2. Minor damage\n");
    print("3. Major damage\n");
    print("4. Not working\n");
    print("Choice: ");
    
    char condition_choice = getchar();
    const char* conditions[] = {"GOOD", "MINOR_DAMAGE", "MAJOR_DAMAGE", "NOT_WORKING"};
    strcpy(trans->condition, conditions[condition_choice - '1']);
    
    // Update records
    trans->actual_return = get_system_time();
    strcpy(item->status, "AVAILABLE");
    
    // Reset location to warehouse
    strcpy(item->location, "WAREHOUSE");
    
    // Check if maintenance needed based on usage
    check_maintenance_needed(item);
    
    // Print check-in confirmation
    print_checkin_confirmation(trans);
    
    log_activity("Equipment checked in",
                "Item: %s, Serial: %s, Condition: %s",
                item->equipment_code, item->serial_number, trans->condition);
    
    print("\nEquipment checked in successfully!\n");
    wait_key();
}

void schedule_maintenance() {
    clear_screen();
    print_header("SCHEDULE MAINTENANCE");
    
    // Check for due maintenance
    uint32_t due_items[50];
    uint32_t due_count = 0;
    
    uint32_t today = get_system_time();
    
    for(uint32_t i = 0; i < MAX_EQUIPMENT_ITEMS; i++) {
        if(strlen(equipment_item_db[i].equipment_code) > 0) {
            if(equipment_item_db[i].maintenance_due ||
               (equipment_item_db[i].next_maintenance > 0 &&
                equipment_item_db[i].next_maintenance <= today)) {
                
                due_items[due_count++] = i;
            }
        }
    }
    
    if(due_count == 0) {
        print("No maintenance due at this time.\n");
    } else {
        print("MAINTENANCE DUE:\n");
        print("%4s %-16s %-20s %-12s %-12s\n",
              "#", "Code", "Equipment", "Serial", "Due Date");
        print("------------------------------------------------------------\n");
        
        for(uint32_t i = 0; i < due_count; i++) {
            uint32_t idx = due_items[i];
            equipment_type_t* type = 
                find_equipment_type(equipment_item_db[idx].equipment_code);
            
            char due_date[11];
            format_date(equipment_item_db[idx].next_maintenance, due_date);
            
            print("%4d %-16s %-20s %-12s %-12s\n",
                  i+1,
                  equipment_item_db[idx].equipment_code,
                  type->name,
                  equipment_item_db[idx].serial_number,
                  due_date);
        }
    }
    
    // Schedule new maintenance
    print("\n1. Schedule preventive maintenance\n");
    print("2. Schedule corrective maintenance\n");
    print("3. View maintenance history\n");
    print("4. Back\n");
    print("\nChoice: ");
    
    char choice = getchar();
    switch(choice) {
        case '1':
            schedule_preventive_maintenance();
            break;
        case '2':
            schedule_corrective_maintenance();
            break;
        case '3':
            view_maintenance_history();
            break;
        case '4':
            return;
    }
}

void schedule_preventive_maintenance() {
    print("\nEnter equipment serial: ");
    char serial[30];
    read_input(serial, 30);
    
    equipment_item_t* item = find_equipment_by_serial(serial);
    if(item == NULL) {
        print("Equipment not found!\n");
        wait_key();
        return;
    }
    
    equipment_type_t* type = find_equipment_type(item->equipment_code);
    
    print("\nEquipment: %s\n", type->name);
    print("Serial: %s\n", item->serial_number);
    print("Last maintenance: %s\n", 
          format_date(item->last_maintenance));
    print("Recommended interval: %d days\n", 
          type->maintenance_interval_days);
    
    print("\nSchedule maintenance for (YYYYMMDD, 0 for today): ");
    uint32_t maintenance_date = read_date();
    if(maintenance_date == 0) maintenance_date = get_system_date();
    
    print("Technician: ");
    char technician[32];
    read_input(technician, 32);
    
    print("Estimated duration (hours): ");
    uint32_t duration = read_uint();
    
    print("Notes: ");
    char notes[128];
    read_input(notes, 128);
    
    // Create maintenance record
    maintenance_record_t* record = find_empty_maintenance_slot();
    record->maintenance_id = generate_maintenance_id();
    record->item_id = item->item_id;
    record->date = maintenance_date;
    strcpy(record->type, "PREVENTIVE");
    strcpy(record->description, notes);
    strcpy(record->technician, technician);
    strcpy(record->status, "SCHEDULED");
    
    // Calculate next maintenance
    record->next_maintenance_date = 
        add_days(maintenance_date, type->maintenance_interval_days);
    
    // Update equipment record
    item->next_maintenance = record->next_maintenance_date;
    item->maintenance_due = 0;
    
    // Print work order
    print_maintenance_work_order(record);
    
    log_activity("Maintenance scheduled",
                "Equipment: %s, Serial: %s, Date: %s",
                item->equipment_code, item->serial_number,
                format_date(maintenance_date));
    
    print("\nMaintenance scheduled successfully!\n");
    wait_key();
}

void inventory_report() {
    clear_screen();
    print_header("EQUIPMENT INVENTORY REPORT");
    
    print("Report as of: %s\n\n", format_datetime(get_system_time()));
    
    // Summary by category
    float total_value = 0;
    uint32_t total_items = 0;
    
    print("=== SUMMARY BY CATEGORY ===\n");
    print("%-20s %8s %12s\n", "Category", "Count", "Total Value");
    print("----------------------------------------\n");
    
    // Group by category
    // Simplified - in real system would use proper grouping
    
    for(uint32_t i = 0; i < MAX_EQUIPMENT_TYPES; i++) {
        if(strlen(equipment_type_db[i].equipment_code) > 0) {
            uint32_t count = 0;
            float value = 0;
            
            for(uint32_t j = 0; j < MAX_EQUIPMENT_ITEMS; j++) {
                if(strcmp(equipment_item_db[j].equipment_code, 
                         equipment_type_db[i].equipment_code) == 0) {
                    count++;
                    value += equipment_item_db[j].current_value;
                }
            }
            
            if(count > 0) {
                print("%-20s %8d $%11.2f\n",
                      equipment_type_db[i].category,
                      count, value);
                
                total_items += count;
                total_value += value;
            }
        }
    }
    
    print("----------------------------------------\n");
    print("%-20s %8d $%11.2f\n\n",
          "TOTAL", total_items, total_value);
    
    // Status summary
    uint32_t status_counts[5] = {0};
    const char* statuses[] = {"AVAILABLE", "IN-USE", "MAINTENANCE", "CALIBRATION", "RETIRED"};
    
    for(uint32_t i = 0; i < MAX_EQUIPMENT_ITEMS; i++) {
        if(strlen(equipment_item_db[i].equipment_code) > 0) {
            for(int j = 0; j < 5; j++) {
                if(strcmp(equipment_item_db[i].status, statuses[j]) == 0) {
                    status_counts[j]++;
                    break;
                }
            }
        }
    }
    
    print("=== STATUS SUMMARY ===\n");
    for(int i = 0; i < 5; i++) {
        print("%-12s: %4d items\n", statuses[i], status_counts[i]);
    }
    
    // Maintenance due
    uint32_t maintenance_due = 0;
    uint32_t calibration_due = 0;
    
    for(uint32_t i = 0; i < MAX_EQUIPMENT_ITEMS; i++) {
        if(equipment_item_db[i].maintenance_due) maintenance_due++;
        if(equipment_item_db[i].calibration_due) calibration_due++;
    }
    
    print("\n=== ALERTS ===\n");
    print("Maintenance due: %d items\n", maintenance_due);
    print("Calibration due: %d items\n", calibration_due);
    
    // Print options
    print("\n1. Print report\n");
    print("2. Export to file\n");
    print("3. Detailed listing\n");
    print("4. Back\n");
    print("\nChoice: ");
    
    char choice = getchar();
    switch(choice) {
        case '1':
            print_report_to_printer();
            break;
        case '2':
            export_report_to_file();
            break;
        case '3':
            detailed_inventory_listing();
            break;
        case '4':
            return;
    }
}

void warehouse_main() {
    // Initialize warehouse module
    load_equipment_database();
    load_maintenance_database();
    load_transaction_database();
    
    // Login
    warehouse_login();
    
    // Main loop
    while(1) {
        clear_screen();
        print_header("EQUIPMENT WAREHOUSE MANAGEMENT");
        
        print_time_date();
        print_warehouse_alerts();
        
        print("\n1. Equipment Check-out\n");
        print("2. Equipment Check-in\n");
        print("3. New Equipment Entry\n");
        print("4. Schedule Maintenance\n");
        print("5. Record Maintenance\n");
        print("6. Inventory Report\n");
        print("7. Asset Tracking\n");
        print("8. Logout\n");
        print("\nSelection: ");
        
        char choice = getchar();
        switch(choice) {
            case '1':
                equipment_checkout();
                break;
            case '2':
                equipment_checkin();
                break;
            case '3':
                new_equipment_entry();
                break;
            case '4':
                schedule_maintenance();
                break;
            case '5':
                record_maintenance();
                break;
            case '6':
                inventory_report();
                break;
            case '7':
                asset_tracking();
                break;
            case '8':
                logout();
                return;
        }
    }
}
