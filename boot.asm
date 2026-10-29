bits 16
org 0x7c00

%ifndef KSECTORS
%define KSECTORS 16
%endif

start:
    cld
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    mov [boot_drive], dl

    mov ah, 0x13
    mov al, 1
    mov bh, 0
    mov bl, 0x0f
    mov cx, 12
    mov dh, 0
    mov dl, 0
    lea bp, [banner]
    int 0x10

    ; load kernel to 0x1000, one sector at a time (CHS, 18 spt / 2 heads)
    mov di, KSECTORS
    mov bx, 0x1000
    mov byte [cur_sec], 2
    mov byte [cur_head], 0
    mov byte [cur_cyl], 0
.load:
    mov si, 3                   ; retries
.retry:
    mov ah, 0x02
    mov al, 1
    mov ch, [cur_cyl]
    mov cl, [cur_sec]
    mov dh, [cur_head]
    mov dl, [boot_drive]
    int 0x13
    jnc .ok
    xor ax, ax
    mov dl, [boot_drive]
    int 0x13
    dec si
    jnz .retry
    jmp disk_error
.ok:
    add bx, 512
    inc byte [cur_sec]
    cmp byte [cur_sec], 19
    jb .next
    mov byte [cur_sec], 1
    inc byte [cur_head]
    cmp byte [cur_head], 2
    jb .next
    mov byte [cur_head], 0
    inc byte [cur_cyl]
.next:
    dec di
    jnz .load

    ; enable A20 (fast gate)
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_start

disk_error:
    mov si, errmsg
.p: lodsb
    test al, al
    jz .h
    mov ah, 0x0e
    int 0x10
    jmp .p
.h: cli
    hlt
    jmp .h

bits 32
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    call 0x1000
.hang:
    cli
    hlt
    jmp .hang

bits 16
align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF       ; code, 4GB, 32-bit
    dq 0x00CF92000000FFFF       ; data, 4GB, 32-bit
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt - 1
    dd gdt

boot_drive: db 0
cur_sec:    db 0
cur_head:   db 0
cur_cyl:    db 0
errmsg:     db "Disk error", 0
banner:
    db "CookieOS 1.0", 0

times 510-($-$$) db 0
dw 0xaa55
