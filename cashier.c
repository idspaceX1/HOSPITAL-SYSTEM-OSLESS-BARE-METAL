#include "pos_system.h"

#define MAX_TRANSACTIONS 10000
#define MAX_INSURANCE_PROVIDERS 20
#define MAX_PAYMENT_METHODS 10

typedef struct {
    uint32_t transaction_id;
    uint32_t patient_id;
    uint32_t date_time;
    char transaction_type[16]; // Consultation, Medication, Lab, Procedure
    float subtotal;
    float discount;
    float tax;
    float total;
    float amount_paid;
    float balance;
    char payment_method[16]; // Cash, Card, Insurance, Mixed
    char status[16]; // Pending, Paid, Partial, Cancelled
    char receipt_number[20];
    char cashier[32];
    uint8_t insurance_claimed;
    char insurance_provider[32];
    char insurance_claim_id[30];
} transaction_t;

typedef struct {
    uint32_t item_id;
    uint32_t transaction_id;
    char item_code[16];
    char description[64];
    uint16_t quantity;
    float unit_price;
    float total;
    uint8_t taxable;
} transaction_item_t;

typedef struct {
    char provider_code[8];
    char provider_name[32];
    float coverage_percentage;
    float max_coverage_per_year;
    float used_coverage;
    uint8_t requires_preauth;
    char contact[64];
    char phone[20];
} insurance_provider_t;

typedef struct {
    uint32_t cashier_id;
    char name[32];
    char till_number[8];
    float cash_float;
    float total_sales;
    uint32_t transaction_count;
    uint8_t logged_in;
    uint32_t login_time;
} cashier_session_t;

// Databases
transaction_t transaction_db[MAX_TRANSACTIONS];
transaction_item_t transaction_items[MAX_TRANSACTIONS * 10]; // 10 items per transaction avg
insurance_provider_t insurance_db[MAX_INSURANCE_PROVIDERS];
cashier_session_t current_cashier;

// Current state
float cash_drawer = 1000.00; // Starting float
uint32_t current_transaction_id = 0;

void cashier_login() {
    clear_screen();
    print_header("CASHIER LOGIN");
    
    char id[8];
    char password[20];
    
    print("Cashier ID: ");
    read_input(id, 8);
    
    print("Password: ");
    read_password(password, 20);
    
    // Validate
    if(strcmp(id, "C001") == 0 && strcmp(password, "cash123") == 0) {
        current_cashier.cashier_id = 1;
        strcpy(current_cashier.name, "Alice Johnson");
        strcpy(current_cashier.till_number, "TILL01");
        current_cashier.cash_float = cash_drawer;
        current_cashier.total_sales = 0;
        current_cashier.transaction_count = 0;
        current_cashier.logged_in = 1;
        current_cashier.login_time = get_system_time();
        
        print("\nLogged in as %s, Till: %s\n", 
              current_cashier.name, current_cashier.till_number);
        print("Cash float: $%.2f\n", current_cashier.cash_float);
        
        log_activity("Cashier login", "Till: %s", current_cashier.till_number);
        wait_key();
    } else {
        print("Invalid credentials!\n");
        delay(2000);
    }
}

void process_payment(uint32_t dispense_id) {
    dispense_record_t* dispense = find_dispense_record(dispense_id);
    if(dispense == NULL) {
        print("Invalid dispense record.\n");
        return;
    }
    
    patient_record_t* patient = find_patient(dispense->patient_id);
    
    clear_screen();
    print_header("PROCESS PAYMENT");
    
    print("Patient: %s %s (ID: %d)\n",
          patient->first_name, patient->last_name, patient->patient_id);
    print("Amount Due: $%.2f\n\n", dispense->net_amount);
    
    // Check insurance
    uint8_t use_insurance = 0;
    if(patient->insurance_type > 0) {
        print("Insurance: %s (%s)\n",
              get_insurance_name(patient->insurance_type),
              patient->insurance_number);
        print("Use insurance? (Y/N): ");
        char use_ins = getchar();
        use_insurance = (use_ins == 'Y' || use_ins == 'y');
    }
    
    float amount_due = dispense->net_amount;
    float insurance_amount = 0;
    float patient_amount = amount_due;
    
    if(use_insurance) {
        insurance_provider_t* ins = 
            &insurance_db[patient->insurance_type - 1];
        
        insurance_amount = amount_due * ins->coverage_percentage / 100;
        if(insurance_amount > ins->max_coverage_per_year - ins->used_coverage) {
            insurance_amount = ins->max_coverage_per_year - ins->used_coverage;
        }
        
        patient_amount = amount_due - insurance_amount;
        
        print("\nInsurance Coverage: %.1f%%\n", ins->coverage_percentage);
        print("Insurance Pays: $%.2f\n", insurance_amount);
        print("Patient Pays: $%.2f\n", patient_amount);
        
        // Update insurance used
        ins->used_coverage += insurance_amount;
    }
    
    // Process patient payment
    if(patient_amount > 0) {
        print("\n=== PATIENT PAYMENT ===\n");
        print("Amount: $%.2f\n", patient_amount);
        
        char payment_method[16];
        float cash_received = 0;
        float card_amount = 0;
        
        print("Payment Method (Cash/Card/Mixed): ");
        read_input(payment_method, 16);
        
        if(strcmp(payment_method, "Cash") == 0 ||
           strcmp(payment_method, "Mixed") == 0) {
            
            print("Cash Received: $");
            cash_received = read_float();
            
            if(cash_received < patient_amount && 
               strcmp(payment_method, "Cash") == 0) {
                print("Insufficient cash!\n");
                return;
            }
            
            float change = cash_received - patient_amount;
            if(change > 0) {
                print("Change: $%.2f\n", change);
                dispense_cash(change);
            }
            
            // Update cash drawer
            cash_drawer += patient_amount;
            current_cashier.cash_float = cash_drawer;
        }
        
        if(strcmp(payment_method, "Card") == 0 ||
           strcmp(payment_method, "Mixed") == 0) {
            
            if(strcmp(payment_method, "Mixed") == 0) {
                card_amount = patient_amount - cash_received;
            } else {
                card_amount = patient_amount;
            }
            
            print("Card Amount: $%.2f\n", card_amount);
            print("Swipe card now...\n");
            
            // Simulate card processing
            if(process_card_payment(card_amount, "MEDICAL")) {
                print("Card payment approved.\n");
            } else {
                print("Card payment failed.\n");
                return;
            }
        }
        
        strcpy(dispense->payment_method, payment_method);
    }
    
    // Create transaction record
    transaction_t* trans = &transaction_db[current_transaction_id];
    trans->transaction_id = current_transaction_id++;
    trans->patient_id = dispense->patient_id;
    trans->date_time = get_system_time();
    strcpy(trans->transaction_type, "MEDICATION");
    trans->subtotal = dispense->total_amount;
    trans->discount = dispense->discount;
    trans->tax = dispense->tax;
    trans->total = dispense->net_amount;
    trans->amount_paid = patient_amount;
    trans->balance = 0;
    strcpy(trans->payment_method, dispense->payment_method);
    strcpy(trans->status, "PAID");
    generate_receipt_number(trans->receipt_number);
    strcpy(trans->cashier, current_cashier.name);
    trans->insurance_claimed = use_insurance;
    
    if(use_insurance) {
        strcpy(trans->insurance_provider, 
               get_insurance_name(patient->insurance_type));
        generate_insurance_claim_id(trans->insurance_claim_id);
    }
    
    // Update dispense record
    dispense->status = 1; // Paid
    
    // Print receipt
    print_receipt(trans->transaction_id);
    
    // Update cashier stats
    current_cashier.total_sales += dispense->net_amount;
    current_cashier.transaction_count++;
    
    log_activity("Payment processed",
                "Receipt: %s, Amount: $%.2f, Method: %s",
                trans->receipt_number, trans->total, trans->payment_method);
}

void print_receipt(uint32_t transaction_id) {
    transaction_t* trans = &transaction_db[transaction_id];
    patient_record_t* patient = find_patient(trans->patient_id);
    
    char buffer[2048];
    
    sprintf(buffer,
        "\n\n"
        "       HOSPITAL RECEIPT\n"
        "       ================\n"
        "Receipt: %s\n"
        "Date: %s\n"
        "Time: %s\n"
        "Cashier: %s\n"
        "Till: %s\n"
        "-------------------------------\n"
        "Patient: %s %s\n"
        "ID: %d\n"
        "-------------------------------\n",
        trans->receipt_number,
        format_date(trans->date_time),
        format_time(trans->date_time),
        trans->cashier,
        current_cashier.till_number,
        patient->first_name,
        patient->last_name,
        patient->patient_id);
    
    // Add items (from dispense record)
    dispense_record_t* dispense = find_dispense_by_patient(trans->patient_id, trans->date_time);
    if(dispense) {
        prescription_t* pres = find_prescription(dispense->prescription_id);
        
        strcat(buffer, "Description: Medication Dispense\n");
        strcat(buffer, "Prescription: ");
        
        char pres_id[12];
        generate_prescription_id(pres->prescription_id, pres_id);
        strcat(buffer, pres_id);
        strcat(buffer, "\n");
        
        strcat(buffer, "-------------------------------\n");
        
        for(int i = 0; i < 5; i++) {
            prescription_item_t* item = 
                &prescription_items[pres->prescription_id * 5 + i];
            if(strlen(item->medication_code) == 0) break;
            
            char item_line[128];
            sprintf(item_line, "%-20s %3d x $%6.2f $%7.2f\n",
                    item->medication_name,
                    item->quantity,
                    item->unit_price,
                    item->total);
            strcat(buffer, item_line);
        }
    }
    
    char totals[256];
    sprintf(totals,
        "-------------------------------\n"
        "Subtotal:           $%8.2f\n"
        "Discount:           $%8.2f\n"
        "Tax:                $%8.2f\n"
        "-------------------------------\n"
        "TOTAL:              $%8.2f\n"
        "-------------------------------\n"
        "Insurance:          $%8.2f\n"
        "Paid by Patient:    $%8.2f\n"
        "Payment Method:     %s\n"
        "-------------------------------\n"
        "Thank you for choosing our hospital!\n"
        "For inquiries call: 1-800-HOSPITAL\n",
        trans->subtotal,
        trans->discount,
        trans->tax,
        trans->total,
        trans->total - trans->amount_paid,
        trans->amount_paid,
        trans->payment_method);
    
    strcat(buffer, totals);
    
    // Print to receipt printer
    parallel_print(buffer);
    
    // Also display on screen
    print("%s", buffer);
    
    wait_key();
}

void end_of_day_report() {
    clear_screen();
    print_header("END OF DAY REPORT");
    
    print("Cashier: %s\n", current_cashier.name);
    print("Till: %s\n", current_cashier.till_number);
    print("Date: %s\n\n", format_date(get_system_time()));
    
    // Calculate totals
    float cash_total = 0;
    float card_total = 0;
    float insurance_total = 0;
    uint32_t transaction_count = 0;
    
    uint32_t today = get_system_time();
    
    for(uint32_t i = 0; i < MAX_TRANSACTIONS; i++) {
        if(transaction_db[i].transaction_id > 0 &&
           is_same_day(transaction_db[i].date_time, today) &&
           strcmp(transaction_db[i].status, "PAID") == 0) {
            
            transaction_count++;
            
            if(strcmp(transaction_db[i].payment_method, "Cash") == 0) {
                cash_total += transaction_db[i].amount_paid;
            } else if(strcmp(transaction_db[i].payment_method, "Card") == 0) {
                card_total += transaction_db[i].amount_paid;
            }
            
            if(transaction_db[i].insurance_claimed) {
                insurance_total += transaction_db[i].total - transaction_db[i].amount_paid;
            }
        }
    }
    
    float total_sales = cash_total + card_total + insurance_total;
    
    print("=== TRANSACTION SUMMARY ===\n");
    print("Total Transactions: %d\n", transaction_count);
    print("Total Sales: $%.2f\n", total_sales);
    print("  - Cash: $%.2f\n", cash_total);
    print("  - Card: $%.2f\n", card_total);
    print("  - Insurance: $%.2f\n", insurance_total);
    
    print("\n=== CASH DRAWER ===\n");
    print("Opening Float: $%.2f\n", 1000.00);
    print("Cash Received: $%.2f\n", cash_total);
    print("Expected Cash: $%.2f\n", 1000.00 + cash_total);
    print("Actual Cash: $");
    
    float actual_cash = read_float();
    
    float variance = actual_cash - (1000.00 + cash_total);
    print("Variance: $%.2f\n", variance);
    
    if(fabs(variance) > 1.00) {
        print("WARNING: Significant variance!\n");
        log_activity("Cash variance", 
                    "Expected: $%.2f, Actual: $%.2f, Diff: $%.2f",
                    1000.00 + cash_total, actual_cash, variance);
    }
    
    // Print Z-Report
    print_z_report(cash_total, card_total, insurance_total, 
                  transaction_count, variance);
    
    // Reset for next day
    cash_drawer = 1000.00;
    current_cashier.cash_float = cash_drawer;
    current_cashier.total_sales = 0;
    current_cashier.transaction_count = 0;
    
    log_activity("End of day report",
                "Total: $%.2f, Transactions: %d",
                total_sales, transaction_count);
}

void cashier_main() {
    // Initialize cashier module
    load_transaction_database();
    load_insurance_database();
    
    // Login
    cashier_login();
    
    // Main loop
    while(1) {
        clear_screen();
        print_header("CASHIER SYSTEM - %s", current_cashier.till_number);
        
        // Check for pending payments (from pharmacy)
        check_pending_payments();
        
        print_time_date();
        print_cashier_status();
        
        print("\n1. Process Payment\n");
        print("2. Manual Transaction\n");
        print("3. Reprint Receipt\n");
        print("4. Void Transaction\n");
        print("5. Cash Drawer\n");
        print("6. End of Day\n");
        print("7. Logout\n");
        print("\nSelection: ");
        
        char choice = getchar();
        switch(choice) {
            case '1':
                process_payment_menu();
                break;
            case '2':
                manual_transaction();
                break;
            case '3':
                reprint_receipt();
                break;
            case '4':
                void_transaction();
                break;
            case '5':
                cash_drawer_management();
                break;
            case '6':
                end_of_day_report();
                break;
            case '7':
                logout();
                return;
        }
    }
}
