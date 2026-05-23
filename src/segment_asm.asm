.CODE

__readcs PROC
    mov ax, cs
    ret
__readcs ENDP

__readds PROC
    mov ax, ds
    ret
__readds ENDP

__reades PROC
    mov ax, es
    ret
__reades ENDP

__readfs PROC
    mov ax, fs
    ret
__readfs ENDP

__readgs PROC
    mov ax, gs
    ret
__readgs ENDP

__readss PROC
    mov ax, ss
    ret
__readss ENDP

__readldtr PROC
    sldt ax
    ret
__readldtr ENDP

__readtr PROC
    str ax
    ret
__readtr ENDP

__lar PROC
    lar eax, ecx
    ret
__lar ENDP

END
