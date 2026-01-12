#include "pos_system.h"

#define MAX_MEDICATIONS 2000
#define MAX_INVENTORY_ITEMS 5000
#define MAX_PHARMACISTS 20

typedef struct {
    char code[16];
    char name[64];
    char generic_name[64];
    char manufacturer[64];
    char drug_class[32];
    char schedule; // I-V
    char form[32]; // Tablet, Capsule, Syrup, etc.
    char strength[32];
    char unit[16];
    float unit_price;
    float wholesale_price;
    uint16_t min_stock;
    uint16_t max_stock;
    uint8_t requires_prescription;
    char storage_conditions[64];
    uint32_t shelf_life_days;
    char barcode[20];
    char ndc_number[20];
} medication_master_t;

typedef struct {
    uint32_t inventory_id;
    char medication_code[16];
    char batch_number[20];
    uint32_t manufacturing_date;
    uint32_t expiration_date;
    uint16_t quantity;
    uint16_t available_quantity;
    char shelf_location[16];
    char supplier[64];
    float purchase_price;
    float selling_price;
    uint8_t status; // 0=inactive, 1=active, 2=quarantine, 3=expired
} inventory_item_t;

typedef struct {
    uint32_t pharmacist_id;
    char license[20];
    char name[32];
    uint8_t access_level;
    uint32_t login_time;
    uint8_t logged_in;
} pharmacist_session_t;

typedef struct {
    uint32_t dispense_id;
    uint32_t prescription_id;
    uint32_t patient_id;
    uint32_t date;
    char pharmacist[32];
    uint8_t status; // 0=pending, 1=dispensed, 2=cancelled
    float total_amount;
    float discount;
    float tax;
    float net_amount;
    char payment_method[16];
    uint8_t insurance_claimed;
} dispense_record_t;

// Databases
medication_master_t medication_db[MAX_MEDICATIONS];
inventory_item_t inventory_db[MAX_INVENTORY_ITEMS];
dispense_record_t dispense_db[10000];
pharmacist_session_t current_pharmacist;

// Inventory management
void check_inventory() {
    clear_screen();
    print_header("INVENTORY CHECK");
    
    // Check low stock
    uint32_t low_stock_count = 0;
    
    print("LOW STOCK ITEMS:\n");
    print("%-6s %-20s %-12s %-8s %-8s %s\n",
          "Code", "Name", "Batch", "Current", "Min", "Location");
    print("------------------------------------------------------------\n");
    
    for(uint32_t i = 0; i < MAX_INVENTORY_ITEMS; i++) {
        if(inventory_db[i].status == 1 && // Active
           inventory_db[i].available_quantity < 
           get_min_stock(inventory_db[i].medication_code)) {
            
            medication_master_t* med = 
                find_medication(inventory_db[i].medication_code);
            
            print("%-6s %-20s %-12s %-8d %-8d %s\n",
                  inventory_db[i].medication_code,
                  med->name,
                  inventory_db[i].batch_number,
                  inventory_db[i].available_quantity,
                  med->min_stock,
                  inventory_db[i].shelf_location);
            
            low_stock_count++;
        }
    }
    
    if(low_stock_count == 0) {
        print("No low stock items.\n");
    }
    
    // Check expiring soon (within 30 days)
    uint32_t expiring_count = 0;
    uint32_t current_date = get_system_time();
    
    print("\nEXPIRING SOON (30 days):\n");
    print("%-6s %-20s %-12s %-8s %-10s %s\n",
          "Code", "Name", "Batch", "Qty", "Expires", "Location");
    print("------------------------------------------------------------\n");
    
    for(uint32_t i = 0; i < MAX_INVENTORY_ITEMS; i++) {
        if(inventory_db[i].status == 1) {
            uint32_t days_to_expire = 
                days_difference(current_date, inventory_db[i].expiration_date);
            
            if(days_to_expire <= 30 && days_to_expire > 0) {
                medication_master_t* med = 
                    find_medication(inventory_db[i].medication_code);
                
                char exp_date[11];
                format_date(inventory_db[i].expiration_date, exp_date);
                
                print("%-6s %-20s %-12s %-8d %-10s %s\n",
                      inventory_db[i].medication_code,
                      med->name,
                      inventory_db[i].batch_number,
                      inventory_db[i].available_quantity,
                      exp_date,
                      inventory_db[i].shelf_location);
                
                expiring_count++;
            }
        }
    }
    
    if(expiring_count == 0) {
        print("No items expiring soon.\n");
    }
    
    wait_key();
}

void process_prescription(uint32_t prescription_id) {
    // Receive prescription from doctor module
    prescription_t* pres = find_prescription(prescription_id);
    if(pres == NULL) {
        print("Prescription not found.\n");
        return;
    }
    
    patient_record_t* patient = find_patient(pres->patient_id);
    
    clear_screen();
    print_header("PROCESS PRESCRIPTION #%08X", prescription_id);
    
    print("Patient: %s %s (ID: %d)\n", 
          patient->first_name, patient->last_name, patient->patient_id);
    print("Doctor: %s\n", get_doctor_name(pres->doctor_id));
    print("Diagnosis: %s\n\n", pres->diagnosis);
    
    // Process each medication
    float total_amount = 0;
    uint8_t all_available = 1;
    
    for(int i = 0; i < 5; i++) {
        prescription_item_t* item = 
            &prescription_items[prescription_id * 5 + i];
        
        if(strlen(item->medication_code) == 0) break;
        
        print("%d. %s - %s\n", i+1, item->medication_code, item->medication_name);
        print("   Dosage: %s, Frequency: %s, Quantity: %d\n",
              item->dosage, item->frequency, item->quantity);
        
        // Check availability
        uint16_t available = check_medication_availability(
            item->medication_code, item->quantity);
        
        if(available >= item->quantity) {
            print("   Status: [AVAILABLE] Stock: %d\n", available);
            
            // Calculate price
            medication_master_t* med = find_medication(item->medication_code);
            item->unit_price = med->unit_price;
            item->total = item->unit_price * item->quantity;
            total_amount += item->total;
            
            print("   Price: $%.2f x %d = $%.2f\n", 
                  item->unit_price, item->quantity, item->total);
        } else {
            print("   Status: [INSUFFICIENT] Available: %d, Required: %d\n",
                  available, item->quantity);
            all_available = 0;
        }
        
        print("\n");
    }
    
    if(!all_available) {
        print("\nSome medications are not available in sufficient quantity.\n");
        print("1. Process available items only\n");
        print("2. Backorder all items\n");
        print("3. Cancel\nChoice: ");
        
        char choice = getchar();
        switch(choice) {
            case '1':
                // Process partial
                break;
            case '2':
                create_backorder(prescription_id);
                return;
            case '3':
                return;
        }
    }
    
    // Create dispense record
    dispense_record_t* dispense = &dispense_db[get_next_dispense_id()];
    dispense->dispense_id = get_next_dispense_id();
    dispense->prescription_id = prescription_id;
    dispense->patient_id = pres->patient_id;
    dispense->date = get_system_time();
    strcpy(dispense->pharmacist, current_pharmacist.name);
    dispense->status = 0; // Pending payment
    
    // Calculate amounts
    dispense->total_amount = total_amount;
    dispense->discount = calculate_discount(pres->patient_id, total_amount);
    dispense->tax = calculate_tax(total_amount - dispense->discount);
    dispense->net_amount = total_amount - dispense->discount + dispense->tax;
    
    print("\n========================================\n");
    print("TOTAL AMOUNT:     $%.2f\n", total_amount);
    print("DISCOUNT:         $%.2f\n", dispense->discount);
    print("TAX:              $%.2f\n", dispense->tax);
    print("NET AMOUNT:       $%.2f\n", dispense->net_amount);
    print("========================================\n");
    
    // Send to cashier
    send_to_cashier(dispense->dispense_id);
    
    // Update prescription status
    pres->status = 2; // Sent to cashier
    
    log_activity("Prescription processed", 
                "Prescription ID: %08X, Amount: $%.2f",
                prescription_id, dispense->net_amount);
}

void dispense_medication(uint32_t dispense_id) {
    dispense_record_t* dispense = find_dispense_record(dispense_id);
    if(dispense == NULL || dispense->status != 1) { // 1 = paid
        print("Dispense record not ready.\n");
        return;
    }
    
    prescription_t* pres = find_prescription(dispense->prescription_id);
    
    clear_screen();
    print_header("DISPENSE MEDICATION #%08X", dispense_id);
    
    // Dispense each item
    for(int i = 0; i < 5; i++) {
        prescription_item_t* item = 
            &prescription_items[pres->prescription_id * 5 + i];
        
        if(strlen(item->medication_code) == 0) break;
        
        // Find inventory batches (FIFO)
        uint16_t remaining = item->quantity;
        
        for(uint32_t j = 0; j < MAX_INVENTORY_ITEMS && remaining > 0; j++) {
            if(inventory_db[j].status == 1 &&
               strcmp(inventory_db[j].medication_code, item->medication_code) == 0 &&
               inventory_db[j].available_quantity > 0) {
                
                uint16_t take = (inventory_db[j].available_quantity < remaining) ?
                               inventory_db[j].available_quantity : remaining;
                
                // Update inventory
                inventory_db[j].available_quantity -= take;
                remaining -= take;
                
                // Record transaction
                record_inventory_transaction(
                    inventory_db[j].inventory_id,
                    TRANSACTION_DISPENSE,
                    take,
                    item->unit_price,
                    dispense_id
                );
                
                // Print label
                print_medication_label(
                    inventory_db[j].medication_code,
                    inventory_db[j].batch_number,
                    item->dosage,
                    item->frequency,
                    item->duration_days,
                    pres->patient_id,
                    get_patient_name(pres->patient_id),
                    format_date(get_system_time())
                );
            }
        }
        
        if(remaining > 0) {
            print("WARNING: Only %d of %d %s dispensed.\n",
                  item->quantity - remaining, item->quantity,
                  item->medication_name);
            create_backorder_item(pres->prescription_id, i, remaining);
        }
        
        item->dispensed = 1;
        item->dispense_date = get_system_time();
    }
    
    // Update records
    dispense->status = 2; // Dispensed
    pres->status = 3; // Dispensed
    
    // Print receipt
    print_dispense_receipt(dispense_id);
    
    log_activity("Medication dispensed",
                "Dispense ID: %08X, Patient ID: %d",
                dispense_id, dispense->patient_id);
}

void receive_supply() {
    clear_screen();
    print_header("RECEIVE SUPPLY");
    
    char invoice_number[20];
    char supplier[64];
    
    print("Invoice Number: ");
    read_input(invoice_number, 20);
    
    print("Supplier: ");
    read_input(supplier, 64);
    
    print("Date: %s\n", format_date(get_system_time()));
    
    uint32_t item_count = 0;
    float total_value = 0;
    
    while(1) {
        print("\nItem %d:\n", item_count + 1);
        
        char med_code[16];
        char batch[20];
        uint32_t mfg_date, exp_date;
        uint16_t quantity;
        float price;
        char location[16];
        
        print("Medication Code: ");
        read_input(med_code, 16);
        
        medication_master_t* med = find_medication(med_code);
        if(med == NULL) {
            print("Medication not in master. Add first.\n");
            continue;
        }
        
        print("Name: %s\n", med->name);
        print("Batch Number: ");
        read_input(batch, 20);
        
        print("Manufacturing Date (YYYYMMDD): ");
        mfg_date = read_date();
        
        print("Expiration Date (YYYYMMDD): ");
        exp_date = read_date();
        
        print("Quantity: ");
        quantity = read_uint();
        
        print("Unit Price: ");
        price = read_float();
        
        print("Shelf Location: ");
        read_input(location, 16);
        
        // Add to inventory
        for(uint32_t i = 0; i < MAX_INVENTORY_ITEMS; i++) {
            if(inventory_db[i].status == 0) { // Empty slot
                inventory_db[i].inventory_id = get_next_inventory_id();
                strcpy(inventory_db[i].medication_code, med_code);
                strcpy(inventory_db[i].batch_number, batch);
                inventory_db[i].manufacturing_date = mfg_date;
                inventory_db[i].expiration_date = exp_date;
                inventory_db[i].quantity = quantity;
                inventory_db[i].available_quantity = quantity;
                strcpy(inventory_db[i].shelf_location, location);
                strcpy(inventory_db[i].supplier, supplier);
                inventory_db[i].purchase_price = price;
                inventory_db[i].selling_price = med->unit_price;
                inventory_db[i].status = 1; // Active
                
                total_value += price * quantity;
                item_count++;
                break;
            }
        }
        
        print("\nAdd another item? (Y/N): ");
        char another = getchar();
        if(another != 'Y' && another != 'y') break;
    }
    
    // Print GRN (Goods Received Note)
    print_goods_received_note(invoice_number, supplier, item_count, total_value);
    
    log_activity("Supply received",
                "Invoice: %s, Items: %d, Value: $%.2f",
                invoice_number, item_count, total_value);
}

void medication_main() {
    // Initialize pharmacy module
    load_medication_database();
    load_inventory_database();
    
    // Login pharmacist
    pharmacist_login();
    
    // Main loop
    while(1) {
        clear_screen();
        print_header("PHARMACY MANAGEMENT");
        
        // Check for new prescriptions (IPC messages)
        check_ipc_messages();
        
        print_time_date();
        print_inventory_summary();
        
        print("\n1. Process Prescription\n");
        print("2. Dispense Medication\n");
        print("3. Inventory Check\n");
        print("4. Receive Supply\n");
        print("5. Stock Adjustment\n");
        print("6. Reports\n");
        print("7. Logout\n");
        print("\nSelection: ");
        
        char choice = getchar();
        switch(choice) {
            case '1':
                process_prescription_menu();
                break;
            case '2':
                dispense_menu();
                break;
            case '3':
                check_inventory();
                break;
            case '4':
                receive_supply();
                break;
            case '5':
                stock_adjustment();
                break;
            case '6':
                generate_reports();
                break;
            case '7':
                logout();
                return;
        }
    }
}
