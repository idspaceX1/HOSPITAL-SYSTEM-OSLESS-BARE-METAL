[BITS 16]
[ORG 0x7C00]

; Boot Sector
jmp short start
nop

; BPB (BIOS Parameter Block)
OEM_ID            db "HOSPPOS "
BytesPerSector    dw 512
SectorsPerCluster db 1
ReservedSectors   dw 1
FATCount          db 2
RootEntries       dw 224
TotalSectors      dw 2880
MediaDescriptor   db 0xF0
SectorsPerFAT     dw 9
SectorsPerTrack   dw 18
HeadsPerCylinder  dw 2
HiddenSectors     dd 0
TotalSectorsLarge dd 0
DriveNumber       db 0
WinNTFlags        db 0
Signature         db 0x29
VolumeID          dd 0x12345678
VolumeLabel       db "HOSPPOS_SYS"
FileSystem        db "FAT12   "

start:
    ; Initialize segments
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    
    ; Save drive number
    mov [DriveNumber], dl
    
    ; Load kernel at 0x1000:0x0000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx
    
    ; Read sectors
    mov ah, 0x02    ; BIOS read sector function
    mov al, 32      ; Number of sectors to read (16KB kernel)
    mov ch, 0       ; Cylinder
    mov cl, 2       ; Sector (starting from 2)
    mov dh, 0       ; Head
    mov dl, [DriveNumber]
    int 0x13
    jc disk_error
    
    ; Load modules
    mov ax, 0x2000
    mov es, ax
    mov bx, 0
    
    mov ah, 0x02
    mov al, 64      ; 32KB for modules
    mov ch, 0
    mov cl, 34      ; Start after kernel
    mov dh, 0
    mov dl, [DriveNumber]
    int 0x13
    
    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]
    
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    jmp CODE_SEG:protected_mode_start

disk_error:
    mov si, disk_error_msg
    call print_string
    hlt

print_string:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

disk_error_msg db "Disk error!", 0

[BITS 32]
protected_mode_start:
    ; Initialize segment registers
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    
    ; Jump to kernel
    jmp 0x10000

; GDT
gdt_start:
    dq 0x0
gdt_code:
    dw 0xFFFF       ; Limit low
    dw 0x0          ; Base low
    db 0x0          ; Base middle
    db 10011010b    ; Access
    db 11001111b    ; Granularity
    db 0x0          ; Base high
gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510-($-$$) db 0
dw 0xAA55
