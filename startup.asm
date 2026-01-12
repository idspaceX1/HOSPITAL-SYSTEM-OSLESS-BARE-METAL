; System Startup Routine

section .text
global _start

_start:
    ; Save multiboot info if present
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx
    
    ; Initialize stack
    mov esp, stack_top
    
    ; Clear BSS section
    xor eax, eax
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, bss_start
    rep stosb
    
    ; Call kernel main
    call kernel_main
    
    ; If kernel returns (shouldn't happen), halt
    cli
.hang:
    hlt
    jmp .hang

section .bss
multiboot_magic resd 1
multiboot_info resd 1
stack_bottom resb 16384
stack_top:
