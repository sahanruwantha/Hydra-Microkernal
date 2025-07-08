section .text
global context_switch

; void context_switch(uint32_t* old_esp, uint32_t* new_esp)
context_switch:
    ; Save current registers and return address
    push ebp
    mov ebp, esp
    
    ; Save other registers
    push ebx
    push esi
    push edi
    
    ; Get the function arguments from the stack
    mov eax, [ebp + 8]     ; old_esp
    mov edx, [ebp + 12]    ; new_esp
    
    ; Save current ESP to old process's PCB
    mov [eax], esp
    
    ; Switch to new stack
    mov esp, [edx]
    
    ; Restore registers
    pop edi
    pop esi
    pop ebx
    pop ebp
    
    ; Return - this will use the new process's return address
    ret 