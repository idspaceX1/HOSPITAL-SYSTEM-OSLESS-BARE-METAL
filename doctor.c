#include "pos_system.h"

#define MAX_PATIENTS 1000
#define MAX_PRESCRIPTIONS 5000
#define MAX_DIAGNOSES 200

typedef struct {
    uint32_t patient_id;
    char first_name[32];
    char last_name[32];
    uint8_t age;
    char gender; // 'M' or 'F'
    char blood_type[4];
    float weight;
    float height;
    char phone[15];
    char address[64];
    char emergency_contact[32];
    char emergency_phone[15];
    uint32_t registration_date;
    uint32_t last_visit;
    uint8_t insurance_type;
    char insurance_number[20];
    uint8_t active;
} patient_record_t;

typedef struct {
    uint32_t prescription_id;
    uint32_t patient_id;
    uint32_t doctor_id;
    uint32_t date;
    char diagnosis[128];
    char symptoms[256];
    char notes[512];
    uint8_t severity; // 1-10
    uint8_t followup_required;
    uint32_t followup_date;
    uint8_t status; // 0=draft, 1=finalized, 2=dispensed
} prescription_t;

typedef struct {
    uint32_t item_id;
    uint32_t prescription_id;
    char medication_code[16];
    char medication_name[64];
    char dosage[32];
    char frequency[32];
    char route[16]; // Oral, IV, IM, etc.
    uint16_t duration_days;
    uint8_t refills_allowed;
    uint8_t refills_used;
    float unit_price;
    uint16_t quantity;
    float total;
    uint8_t dispensed;
    uint32_t dispense_date;
} prescription_item_t;

typedef struct {
    uint32_t doctor_id;
    char license_number[20];
    char first_name[32];
    char last_name[32];
    char specialization[32];
    char department[32];
    uint8_t access_level;
    char signature_path[64];
    uint32_t login_timestamp;
    uint8_t logged_in;
} doctor_session_t;

// Database Storage
patient_record_t patient_db[MAX_PATIENTS];
prescription_t prescription_db[MAX_PRESCRIPTIONS];
prescription_item_t prescription_items[MAX_PRESCRIPTIONS * 5]; // 5 items per prescription avg
doctor_session_t current_doctor;

// Current state
uint32_t current_patient_id = 0;
uint32_t current_prescription_id = 0;
uint8_t current_screen = 0; // 0=login, 1=search, 2=patient, 3=prescription

void doctor_login() {
    clear_screen();
    print_header("DOCTOR LOGIN");
    
    char license[20];
    char password[20];
    
    print("License Number: ");
    read_input(license, 20);
    
    print("Password: ");
    read_password(password, 20);
    
    // Validate credentials (hardcoded for demo)
    if(strcmp(license, "MD123456") == 0 && strcmp(password, "secure123") == 0) {
        current_doctor.doctor_id = 1;
        strcpy(current_doctor.license_number, "MD123456");
        strcpy(current_doctor.first_name, "John");
        strcpy(current_doctor.last_name, "Smith");
        strcpy(current_doctor.specialization, "Cardiology");
        strcpy(current_doctor.department, "Emergency");
        current_doctor.access_level = 9;
        current_doctor.login_timestamp = get_system_time();
        current_doctor.logged_in = 1;
        
        log_activity("Doctor login", "Successful login", 1);
        main_menu();
    } else {
        print("Invalid credentials!");
        delay(2000);
        doctor_login();
    }
}

void main_menu() {
    while(1) {
        clear_screen();
        print_header("MAIN MENU - DR. %s %s", 
                    current_doctor.first_name, 
                    current_doctor.last_name);
        
        print_time_date();
        
        print("\n1. Search Patient\n");
        print("2. New Patient Registration\n");
        print("3. Create Prescription\n");
        print("4. View History\n");
        print("5. Statistics\n");
        print("6. Logout\n");
        print("\nSelection: ");
        
        char choice = getchar();
        switch(choice) {
            case '1':
                search_patient();
                break;
            case '2':
                register_patient();
                break;
            case '3':
                create_prescription();
                break;
            case '4':
                view_history();
                break;
            case '5':
                show_statistics();
                break;
            case '6':
                logout();
                return;
        }
    }
}

void search_patient() {
    clear_screen();
    print_header("PATIENT SEARCH");
    
    char search_term[32];
    print("Search (ID/Name/Phone): ");
    read_input(search_term, 32);
    
    // Search algorithm
    uint32_t results[50];
    uint32_t result_count = 0;
    
    for(uint32_t i = 0; i < MAX_PATIENTS; i++) {
        if(patient_db[i].active) {
            // Check ID
            char id_str[10];
            int_to_str(patient_db[i].patient_id, id_str);
            if(strstr(id_str, search_term) != NULL) {
                results[result_count++] = i;
                continue;
            }
            
            // Check name
            char full_name[65];
            strcpy(full_name, patient_db[i].first_name);
            strcat(full_name, " ");
            strcat(full_name, patient_db[i].last_name);
            if(strstr(full_name, search_term) != NULL) {
                results[result_count++] = i;
                continue;
            }
            
            // Check phone
            if(strstr(patient_db[i].phone, search_term) != NULL) {
                results[result_count++] = i;
            }
        }
    }
    
    if(result_count == 0) {
        print("\nNo patients found.\n");
        wait_key();
        return;
    }
    
    // Display results
    print("\n%4s %-20s %-12s %-6s %s\n", 
          "ID", "Name", "Phone", "Age", "Last Visit");
    print("------------------------------------------------------------\n");
    
    for(uint32_t i = 0; i < result_count && i < 20; i++) {
        uint32_t idx = results[i];
        print("%4d %-20s %-12s %-6d ", 
              patient_db[idx].patient_id,
              patient_db[idx].last_name,
              patient_db[idx].phone,
              patient_db[idx].age);
        
        // Format date
        char date_str[11];
        format_date(patient_db[idx].last_visit, date_str);
        print("%s\n", date_str);
    }
    
    print("\nSelect patient ID (0 to cancel): ");
    uint32_t selected_id = read_uint();
    
    if(selected_id == 0) return;
    
    // Find and display patient
    for(uint32_t i = 0; i < MAX_PATIENTS; i++) {
        if(patient_db[i].patient_id == selected_id) {
            display_patient_details(i);
            break;
        }
    }
}

void create_prescription() {
    if(current_patient_id == 0) {
        print("No patient selected. Search patient first.\n");
        wait_key();
        return;
    }
    
    clear_screen();
    print_header("NEW PRESCRIPTION");
    
    // Get patient info
    patient_record_t* patient = find_patient(current_patient_id);
    
    print("Patient: %s %s (ID: %d)\n", 
          patient->first_name, patient->last_name, patient->patient_id);
    print("Age: %d, Gender: %c, Weight: %.1f kg, Height: %.1f cm\n\n",
          patient->age, patient->gender, patient->weight, patient->height);
    
    // Create prescription header
    prescription_t* pres = &prescription_db[current_prescription_id];
    pres->prescription_id = current_prescription_id++;
    pres->patient_id = current_patient_id;
    pres->doctor_id = current_doctor.doctor_id;
    pres->date = get_system_time();
    pres->status = 0; // Draft
    
    print("Diagnosis: ");
    read_input(pres->diagnosis, 128);
    
    print("Symptoms: ");
    read_input(pres->symptoms, 256);
    
    print("Notes: ");
    read_input(pres->notes, 512);
    
    print("Severity (1-10): ");
    pres->severity = read_uint();
    
    print("Follow-up required? (Y/N): ");
    char followup = getchar();
    pres->followup_required = (followup == 'Y' || followup == 'y') ? 1 : 0;
    
    if(pres->followup_required) {
        print("Follow-up in (days): ");
        uint16_t days = read_uint();
        pres->followup_date = add_days(pres->date, days);
    }
    
    // Medication entry
    prescription_item_loop(pres->prescription_id);
    
    // Finalize
    print("\n1. Save Draft\n2. Finalize\n3. Cancel\nChoice: ");
    char choice = getchar();
    
    switch(choice) {
        case '1':
            pres->status = 0;
            print("Prescription saved as draft.\n");
            break;
        case '2':
            pres->status = 1;
            print("Prescription finalized.\n");
            
            // Generate prescription ID with checksum
            char pres_id[12];
            generate_prescription_id(pres->prescription_id, pres_id);
            
            // Print prescription
            print_prescription(pres->prescription_id);
            
            // Send to pharmacy
            send_to_pharmacy(pres->prescription_id);
            break;
        case '3':
            // Cancel
            break;
    }
    
    log_activity("Prescription created", 
                "Patient ID: %d, Prescription ID: %d",
                current_patient_id, pres->prescription_id);
}

void prescription_item_loop(uint32_t prescription_id) {
    uint32_t item_count = 0;
    
    print("\n=== MEDICATION ENTRY ===\n");
    
    while(1) {
        print("\nItem %d:\n", item_count + 1);
        
        prescription_item_t* item = 
            &prescription_items[prescription_id * 5 + item_count];
        item->prescription_id = prescription_id;
        item->item_id = item_count;
        
        // Search medication
        print("Medication code/name: ");
        char search[32];
        read_input(search, 32);
        
        // Query medication database
        medication_t* med = search_medication(search);
        if(med == NULL) {
            print("Medication not found. Try again.\n");
            continue;
        }
        
        strcpy(item->medication_code, med->code);
        strcpy(item->medication_name, med->name);
        
        print("Selected: %s - %s\n", med->code, med->name);
        print("Available forms: %s\n", med->available_forms);
        
        print("Dosage: ");
        read_input(item->dosage, 32);
        
        print("Frequency: ");
        read_input(item->frequency, 32);
        
        print("Route (Oral/IV/IM/Topical): ");
        read_input(item->route, 16);
        
        print("Duration (days): ");
        item->duration_days = read_uint();
        
        print("Quantity: ");
        item->quantity = read_uint();
        
        print("Refills allowed: ");
        item->refills_allowed = read_uint();
        
        // Calculate price
        item->unit_price = med->unit_price;
        item->total = item->unit_price * item->quantity;
        
        print("Price: $%.2f x %d = $%.2f\n", 
              item->unit_price, item->quantity, item->total);
        
        item_count++;
        
        print("\nAdd another medication? (Y/N): ");
        char another = getchar();
        if(another != 'Y' && another != 'y') break;
        
        if(item_count >= 5) {
            print("Maximum 5 medications per prescription.\n");
            break;
        }
    }
}

void print_prescription(uint32_t prescription_id) {
    // Format prescription for printing
    char buffer[2048];
    prescription_t* pres = &prescription_db[prescription_id];
    patient_record_t* patient = find_patient(pres->patient_id);
    
    sprintf(buffer, 
        "========================================\n"
        "          HOSPITAL PRESCRIPTION        \n"
        "========================================\n"
        "Prescription ID: %08X\n"
        "Date: %s\n"
        "Patient: %s %s\n"
        "ID: %d, Age: %d, Gender: %c\n"
        "Doctor: %s %s, %s\n"
        "License: %s\n"
        "========================================\n"
        "DIAGNOSIS: %s\n"
        "SYMPTOMS: %s\n"
        "NOTES: %s\n"
        "========================================\n"
        "MEDICATIONS:\n",
        pres->prescription_id,
        format_date(pres->date),
        patient->first_name, patient->last_name,
        patient->patient_id, patient->age, patient->gender,
        current_doctor.first_name, current_doctor.last_name,
        current_doctor.specialization,
        current_doctor.license_number,
        pres->diagnosis,
        pres->symptoms,
        pres->notes);
    
    // Add medications
    for(int i = 0; i < 5; i++) {
        prescription_item_t* item = 
            &prescription_items[prescription_id * 5 + i];
        if(strlen(item->medication_code) == 0) break;
        
        char item_buf[256];
        sprintf(item_buf,
            "%d. %s - %s\n"
            "   Dosage: %s, Frequency: %s\n"
            "   Route: %s, Duration: %d days\n"
            "   Quantity: %d, Refills: %d\n"
            "   Price: $%.2f\n",
            i + 1,
            item->medication_code,
            item->medication_name,
            item->dosage,
            item->frequency,
            item->route,
            item->duration_days,
            item->quantity,
            item->refills_allowed,
            item->total);
        
        strcat(buffer, item_buf);
    }
    
    strcat(buffer,
        "========================================\n"
        "Doctor's Signature: ___________________\n"
        "\n"
        "FOR PHARMACY USE:\n"
        "Dispensed: __________ Date: __________\n"
        "Pharmacist: ___________________________\n");
    
    // Send to printer
    parallel_print(buffer);
    
    // Also save to file
    save_prescription_file(prescription_id, buffer);
}

void send_to_pharmacy(uint32_t prescription_id) {
    // Create IPC message
    ipc_message_t msg;
    msg.sender = MODULE_DOCTOR;
    msg.receiver = MODULE_MEDICATION;
    msg.message_type = MSG_NEW_PRESCRIPTION;
    msg.data_size = sizeof(prescription_id);
    memcpy(msg.data, &prescription_id, sizeof(prescription_id));
    
    send_ipc_message(&msg);
    
    print("Prescription sent to pharmacy.\n");
}

void doctor_main() {
    // Initialize doctor module
    load_patient_database();
    load_prescription_database();
    
    // Main loop
    doctor_login();
    
    // Cleanup
    save_databases();
}
