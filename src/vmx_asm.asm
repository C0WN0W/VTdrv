.CODE

EXTERN HvVmExitHandler:PROC

AsmVmxLaunch PROC
    pushfq
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rcx, rsp
    sub rsp, 20h
    call HvVmExitHandler
    add rsp, 20h

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    popfq

    vmresume
    jz VmxError
    jc VmxError

    xor rax, rax
    ret

VmxError:
    mov rax, 1
    ret
AsmVmxLaunch ENDP

AsmVmxCall PROC
    vmcall
    ret
AsmVmxCall ENDP

AsmInvept PROC
    invept rcx, oword ptr [rdx]
    jz InveptError
    jc InveptError
    xor rax, rax
    ret

InveptError:
    mov rax, 1
    ret
AsmInvept ENDP

AsmInvvpid PROC
    invvpid rcx, oword ptr [rdx]
    jz InvvpidError
    jc InvvpidError
    xor rax, rax
    ret

InvvpidError:
    mov rax, 1
    ret
AsmInvvpid ENDP

AsmVmxSaveState PROC
    mov [rcx + 0h], rax
    mov [rcx + 8h], rcx
    mov [rcx + 10h], rdx
    mov [rcx + 18h], rbx
    mov [rcx + 20h], rsp
    mov [rcx + 28h], rbp
    mov [rcx + 30h], rsi
    mov [rcx + 38h], rdi
    mov [rcx + 40h], r8
    mov [rcx + 48h], r9
    mov [rcx + 50h], r10
    mov [rcx + 58h], r11
    mov [rcx + 60h], r12
    mov [rcx + 68h], r13
    mov [rcx + 70h], r14
    mov [rcx + 78h], r15
    ret
AsmVmxSaveState ENDP

AsmVmxRestoreState PROC
    mov rax, [rcx + 0h]
    mov rdx, [rcx + 10h]
    mov rbx, [rcx + 18h]
    mov rsp, [rcx + 20h]
    mov rbp, [rcx + 28h]
    mov rsi, [rcx + 30h]
    mov rdi, [rcx + 38h]
    mov r8, [rcx + 40h]
    mov r9, [rcx + 48h]
    mov r10, [rcx + 50h]
    mov r11, [rcx + 58h]
    mov r12, [rcx + 60h]
    mov r13, [rcx + 68h]
    mov r14, [rcx + 70h]
    mov r15, [rcx + 78h]
    mov rcx, [rcx + 8h]
    ret
AsmVmxRestoreState ENDP

END
