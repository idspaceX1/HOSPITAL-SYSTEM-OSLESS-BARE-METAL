#include "pos_system.h"

#define MAX_APPOINTMENTS 5000
#define MAX_DEPARTMENTS 20
#define MAX_DOCTOR_SCHEDULES 100

typedef struct {
    uint32_t appointment_id;
    uint32_t patient_id;
    uint32_t doctor_id;
    uint32_t date_time;
    char department[32];
    char reason[64];
    uint8_t urgency; // 1-5
    char status[16]; // Scheduled, Checked-in, In-progress, Completed, Cancelled
    uint32_t checkin_time;
    uint32_t start_time;
    uint32_t end_time;
    char notes[128];
    uint8_t new_patient;
    float consultation_fee;
} appointment_t;

typedef struct {
    char department_code[8];
    char department_name[32];
    char location[32];
    char phone_extension[8];
    uint8_t max_patients_per_day;
    uint8_t current_patients_today;
    uint32_t open_time; // HHMM format
    uint32_t close_time; // HHMM format
} department_t;

typedef struct {
    uint32_t doctor_id;
    uint32_t day_of_week; // 0-6
    uint32_t start_time;
    uint32_t end_time;
    uint16_t slot_duration; // minutes
    uint8_t max_appointments;
    uint8_t is_available;
} doctor_schedule_t;

typedef struct {
    uint32_t receptionist_id;
    char name[32];
    uint8_t access_level;
    uint32_t login_time;
    uint8_t logged_in;
} receptionist_session_t;

// Databases
appointment_t appointment_db[MAX_APPOINTMENTS];
department_t department_db[MAX_DEPARTMENTS];
doctor_schedule_t schedule_db[MAX_DOCTOR_SCHEDULES];
receptionist_session_t current_receptionist;

// Queue management
uint32_t waiting_queue[100];
uint32_t queue_front = 0;
uint32_t queue_rear = 0;
uint32_t queue_size = 0;

void new_patient_registration() {
    clear_screen();
    print_header("NEW PATIENT REGISTRATION");
    
    // Find empty slot in patient database
    uint32_t patient_index = 0;
    for(; patient_index < MAX_PATIENTS; patient_index++) {
        if(!patient_db[patient_index].active) break;
    }
    
    if(patient_index >= MAX_PATIENTS) {
        print("Patient database full!\n");
        wait_key();
        return;
    }
    
    patient_record_t* patient = &patient_db[patient_index];
    
    // Generate patient ID
    patient->patient_id = generate_patient_id();
    patient->registration_date = get_system_time();
    patient->last_visit = patient->registration_date;
    patient->active = 1;
    
    print("New Patient ID: %d\n\n", patient->patient_id);
    
    // Collect information
    print("First Name: ");
    read_input(patient->first_name, 32);
    
    print("Last Name: ");
    read_input(patient->last_name, 32);
    
    print("Date of Birth (YYYYMMDD): ");
    uint32_t dob = read_date();
    patient->age = calculate_age(dob);
    
    print("Gender (M/F): ");
    patient->gender = getchar();
    getchar(); // Consume newline
    
    print("Blood Type: ");
    read_input(patient->blood_type, 4);
    
    print("Weight (kg): ");
    patient->weight = read_float();
    
    print("Height (cm): ");
    patient->height = read_float();
    
    print("Phone: ");
    read_input(patient->phone, 15);
    
    print("Address: ");
    read_input(patient->address, 64);
    
    print("Emergency Contact: ");
    read_input(patient->emergency_contact, 32);
    
    print("Emergency Phone: ");
    read_input(patient->emergency_phone, 15);
    
    print("\n=== INSURANCE INFORMATION ===\n");
    print("Insurance Type:\n");
    print("1. None\n");
    print("2. Basic\n");
    print("3. Premium\n");
    print("4. Government\n");
    print("Choice: ");
    
    patient->insurance_type = read_uint();
    
    if(patient->insurance_type > 1) {
        print("Insurance Number: ");
        read_input(patient->insurance_number, 20);
    }
    
    // Assign to a department (default: General Medicine)
    strcpy(patient->department_assigned, "GENERAL");
    
    // Print registration card
    print_registration_card(patient);
    
    // Add to today's queue
    add_to_queue(patient->patient_id);
    
    log_activity("New patient registered",
                "ID: %d, Name: %s %s",
                patient->patient_id, 
                patient->first_name, 
                patient->last_name);
    
    print("\nRegistration complete! Patient added to queue.\n");
    wait_key();
}

void schedule_appointment() {
    clear_screen();
    print_header("SCHEDULE APPOINTMENT");
    
    // Get patient
    uint32_t patient_id;
    print("Patient ID (0 for new): ");
    patient_id = read_uint();
    
    if(patient_id == 0) {
        new_patient_registration();
        patient_id = patient_db[find_last_patient_index()].patient_id;
    } else {
        if(!validate_patient_id(patient_id)) {
            print("Invalid patient ID!\n");
            wait_key();
            return;
        }
    }
    
    // Select department
    print("\n=== DEPARTMENTS ===\n");
    for(int i = 0; i < MAX_DEPARTMENTS; i++) {
        if(strlen(department_db[i].department_code) > 0) {
            print("%d. %s - %s\n", 
                  i+1, 
                  department_db[i].department_code,
                  department_db[i].department_name);
        }
    }
    
    print("\nSelect department: ");
    uint32_t dept_choice = read_uint();
    if(dept_choice < 1 || dept_choice > MAX_DEPARTMENTS) {
        print("Invalid selection!\n");
        wait_key();
        return;
    }
    
    department_t* dept = &department_db[dept_choice - 1];
    
    // Check department capacity
    if(dept->current_patients_today >= dept->max_patients_per_day) {
        print("Department at full capacity for today!\n");
        print("Try another department or schedule for another day.\n");
        wait_key();
        return;
    }
    
    // Select doctor
    print("\n=== AVAILABLE DOCTORS ===\n");
    uint32_t available_doctors[10];
    uint32_t doc_count = 0;
    
    // Find doctors in this department with available slots
    // Simplified - in real system would check schedule
    
    print("Select doctor ID: ");
    uint32_t doctor_id = read_uint();
    
    // Select date
    print("\nAppointment Date (YYYYMMDD, 0 for today): ");
    uint32_t appt_date = read_date();
    if(appt_date == 0) appt_date = get_system_date();
    
    // Select time slot
    print("\nAvailable time slots:\n");
    
    // Generate available slots based on doctor's schedule
    uint32_t slots[10];
    uint32_t slot_count = get_available_slots(doctor_id, appt_date, slots, 10);
    
    if(slot_count == 0) {
        print("No available slots for selected date.\n");
        wait_key();
        return;
    }
    
    for(uint32_t i = 0; i < slot_count; i++) {
        char time_str[6];
        format_time_hm(slots[i], time_str);
        print("%d. %s\n", i+1, time_str);
    }
    
    print("\nSelect time slot: ");
    uint32_t slot_choice = read_uint();
    if(slot_choice < 1 || slot_choice > slot_count) {
        print("Invalid selection!\n");
        wait_key();
        return;
    }
    
    uint32_t appt_time = slots[slot_choice - 1];
    
    // Get reason
    char reason[64];
    print("Reason for visit: ");
    read_input(reason, 64);
    
    print("Urgency (1-5): ");
    uint8_t urgency = read_uint();
    
    // Create appointment
    appointment_t* appt = find_empty_appointment_slot();
    if(appt == NULL) {
        print("Appointment database full!\n");
        wait_key();
        return;
    }
    
    appt->appointment_id = generate_appointment_id();
    appt->patient_id = patient_id;
    appt->doctor_id = doctor_id;
    appt->date_time = combine_date_time(appt_date, appt_time);
    strcpy(appt->department, dept->department_code);
    strcpy(appt->reason, reason);
    appt->urgency = urgency;
    strcpy(appt->status, "SCHEDULED");
    appt->new_patient = is_new_patient(patient_id);
    appt->consultation_fee = calculate_consultation_fee(doctor_id, appt->new_patient);
    
    // Update department count
    dept->current_patients_today++;
    
    // Print appointment slip
    print_appointment_slip(appt);
    
    log_activity("Appointment scheduled",
                "Patient: %d, Doctor: %d, Time: %s",
                patient_id, doctor_id, format_datetime(appt->date_time));
    
    print("\nAppointment scheduled successfully!\n");
    wait_key();
}

void patient_checkin() {
    clear_screen();
    print_header("PATIENT CHECK-IN");
    
    print("1. By Appointment ID\n");
    print("2. By Patient ID\n");
    print("3. Walk-in\n");
    print("\nChoice: ");
    
    char choice = getchar();
    
    switch(choice) {
        case '1':
            checkin_by_appointment();
            break;
        case '2':
            checkin_by_patient();
            break;
        case '3':
            walkin_checkin();
            break;
        default:
            return;
    }
}

void checkin_by_appointment() {
    print("\nAppointment ID: ");
    uint32_t appt_id = read_uint();
    
    appointment_t* appt = find_appointment(appt_id);
    if(appt == NULL) {
        print("Appointment not found!\n");
        wait_key();
        return;
    }
    
    if(strcmp(appt->status, "SCHEDULED") != 0) {
        print("Appointment status: %s\n", appt->status);
        if(strcmp(appt->status, "CHECKED-IN") == 0) {
            print("Patient already checked in.\n");
        }
        wait_key();
        return;
    }
    
    // Verify patient
    patient_record_t* patient = find_patient(appt->patient_id);
    
    print("\nPatient: %s %s\n", patient->first_name, patient->last_name);
    print("Appointment Time: %s\n", format_datetime(appt->date_time));
    print("Doctor: %s\n", get_doctor_name(appt->doctor_id));
    print("Confirm check-in? (Y/N): ");
    
    char confirm = getchar();
    if(confirm == 'Y' || confirm == 'y') {
        appt->checkin_time = get_system_time();
        strcpy(appt->status, "CHECKED-IN");
        
        // Add to queue
        add_to_queue(patient->patient_id);
        
        // Print check-in slip
        print_checkin_slip(appt);
        
        log_activity("Patient checked in",
                    "Appointment: %d, Patient: %d",
                    appt_id, patient->patient_id);
        
        print("\nCheck-in successful! Queue number: %d\n", queue_size);
    }
    
    wait_key();
}

void queue_management() {
    clear_screen();
    print_header("QUEUE MANAGEMENT");
    
    print("Current Queue: %d patients\n\n", queue_size);
    
    if(queue_size > 0) {
        print("=== WAITING ===\n");
        for(uint32_t i = 0; i < queue_size; i++) {
            uint32_t idx = (queue_front + i) % 100;
            uint32_t patient_id = waiting_queue[idx];
            patient_record_t* patient = find_patient(patient_id);
            
            if(patient) {
                appointment_t* appt = find_appointment_by_patient_today(patient_id);
                
                print("%3d. %-20s ID: %d", 
                      i+1, 
                      patient->last_name,
                      patient_id);
                
                if(appt) {
                    print(" - Dr. %s", get_doctor_shortname(appt->doctor_id));
                }
                
                print("\n");
            }
        }
    }
    
    print("\n1. Call Next Patient\n");
    print("2. Remove from Queue\n");
    print("3. View Department Queues\n");
    print("4. Refresh\n");
    print("5. Back\n");
    print("\nChoice: ");
    
    char choice = getchar();
    switch(choice) {
        case '1':
            call_next_patient();
            break;
        case '2':
            remove_from_queue();
            break;
        case '3':
            view_department_queues();
            break;
        case '4':
            break;
        case '5':
            return;
    }
    
    queue_management();
}

void call_next_patient() {
    if(queue_size == 0) {
        print("Queue is empty!\n");
        wait_key();
        return;
    }
    
    uint32_t patient_id = waiting_queue[queue_front];
    patient_record_t* patient = find_patient(patient_id);
    
    // Find appointment
    appointment_t* appt = find_appointment_by_patient_today(patient_id);
    
    clear_screen();
    print_header("CALL PATIENT");
    
    print("NEXT PATIENT:\n");
    print("Name: %s %s\n", patient->first_name, patient->last_name);
    print("Patient ID: %d\n", patient_id);
    
    if(appt) {
        print("Doctor: %s\n", get_doctor_name(appt->doctor_id));
        print("Department: %s\n", appt->department);
        print("Room: %s\n", get_doctor_room(appt->doctor_id));
        
        // Update appointment status
        appt->start_time = get_system_time();
        strcpy(appt->status, "IN-PROGRESS");
    }
    
    // Remove from queue
    queue_front = (queue_front + 1) % 100;
    queue_size--;
    
    // Display on queue board (if connected)
    update_queue_display();
    
    // Sound bell
    sound_bell();
    
    log_activity("Patient called",
                "Patient: %d, Name: %s",
                patient_id, patient->last_name);
    
    print("\nPatient called to consultation room.\n");
    wait_key();
}

void reception_main() {
    // Initialize reception module
    load_appointment_database();
    load_department_database();
    load_schedule_database();
    
    // Login
    receptionist_login();
    
    // Main loop
    while(1) {
        clear_screen();
        print_header("RECEPTION SYSTEM");
        
        print_time_date();
        print_queue_status();
        print_todays_appointments();
        
        print("\n1. New Patient Registration\n");
        print("2. Schedule Appointment\n");
        print("3. Patient Check-in\n");
        print("4. Queue Management\n");
        print("5. Appointment Lookup\n");
        print("6. Patient Information\n");
        print("7. Daily Report\n");
        print("8. Logout\n");
        print("\nSelection: ");
        
        char choice = getchar();
        switch(choice) {
            case '1':
                new_patient_registration();
                break;
            case '2':
                schedule_appointment();
                break;
            case '3':
                patient_checkin();
                break;
            case '4':
                queue_management();
                break;
            case '5':
                appointment_lookup();
                break;
            case '6':
                patient_information();
                break;
            case '7':
                daily_report();
                break;
            case '8':
                logout();
                return;
        }
    }
}
