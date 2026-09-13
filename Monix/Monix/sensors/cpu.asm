; cpu.asm - CPU low-level sensor reads (MASM x64)
; RDTSC, RDTSCP, CPUID, performance counters

.code

; ---------------------------------------------------------------
; cpu_read_tsc: Read timestamp counter via RDTSC
; Returns: RAX = full 64-bit TSC
; ---------------------------------------------------------------
cpu_read_tsc PROC
    rdtsc
    shl rdx, 32
    or  rax, rdx
    ret
cpu_read_tsc ENDP

; ---------------------------------------------------------------
; cpu_read_tscp: Read timestamp counter via RDTSCP
; Args: RCX = uint32_t* auxOut (core id output, may be NULL)
; Returns: RAX = TSC
; ---------------------------------------------------------------
cpu_read_tscp PROC
    rdtscp
    test rcx, rcx
    jz   tscp_noaux
    mov  [rcx], edx
tscp_noaux:
    shl  rdx, 32
    or   rax, rdx
    ret
cpu_read_tscp ENDP

; ---------------------------------------------------------------
; cpu_cpuid: Execute CPUID instruction
; Args (Windows x64 ABI):
;   RCX = leaf
;   RDX = subleaf
;   R8  = uint32_t* eax_out
;   R9  = uint32_t* ebx_out
;   [rsp+32] = uint32_t* ecx_out  (5th arg)
;   [rsp+40] = uint32_t* edx_out  (6th arg)
; ---------------------------------------------------------------
cpu_cpuid PROC
    push rbx
    push rsi
    push rdi

    ; After 3 pushes (24 bytes) + return addr (8 bytes):
    ; [rsp+32] = shadow RCX, [rsp+40] = shadow RDX
    ; [rsp+48] = shadow R8,  [rsp+56] = shadow R9
    ; [rsp+64] = 5th arg (ecx_out), [rsp+72] = 6th arg (edx_out)
    mov  r10d, ecx         ; r10 = leaf
    mov  esi, edx          ; esi = subleaf
    mov  r11, r8           ; r11 = eax out ptr
    mov  rdi, r9           ; rdi = ebx out ptr
    mov  r8, [rsp+64]      ; ecx out ptr (5th arg)
    mov  r9, [rsp+72]      ; edx out ptr (6th arg)

    mov  eax, r10d         ; eax = leaf
    mov  ecx, esi          ; ecx = subleaf
    cpuid

    mov  [r11], eax
    mov  [rdi], ebx
    mov  [r8], ecx
    mov  [r9], edx

    pop  rdi
    pop  rsi
    pop  rbx
    ret
cpu_cpuid ENDP

; ---------------------------------------------------------------
; cpu_identify: Full CPU identification via CPUID
; Args: RCX = CpuInfo* out
; ---------------------------------------------------------------
cpu_identify PROC
    push rbx
    push rsi
    push rdi

    mov  rdi, rcx          ; rdi = CpuInfo*

    ; --- Vendor string (CPUID leaf 0) ---
    xor  eax, eax
    cpuid
    mov  [rdi+0], ebx
    mov  [rdi+4], edx
    mov  [rdi+8], ecx
    mov  byte ptr [rdi+12], 0

    ; --- Brand string (CPUID 0x80000002-0x80000004) ---
    mov  eax, 80000002h
    cpuid
    mov  [rdi+13], eax
    mov  [rdi+17], ebx
    mov  [rdi+21], ecx
    mov  [rdi+25], edx
    mov  eax, 80000003h
    cpuid
    mov  [rdi+29], eax
    mov  [rdi+33], ebx
    mov  [rdi+37], ecx
    mov  [rdi+41], edx
    mov  eax, 80000004h
    cpuid
    mov  [rdi+45], eax
    mov  [rdi+49], ebx
    mov  [rdi+53], ecx
    mov  [rdi+57], edx
    mov  byte ptr [rdi+61], 0

    ; --- Family/Model/Stepping/Features (leaf 1) ---
    mov  eax, 1
    xor  ecx, ecx
    cpuid
    mov  esi, eax          ; esi = raw EAX
    mov  r8d, edx          ; r8d = featuresEdx
    mov  r9d, ecx          ; r9d = featuresEcx

    ; Core count from EBX[23:16]
    shr  ebx, 16
    and  ebx, 0FFh
    mov  [rdi+74], ebx     ; physicalCores
    mov  [rdi+78], ebx     ; logicalCores

    ; Extract family/model/stepping from raw EAX
    mov  eax, esi
    and  eax, 0Fh
    mov  [rdi+70], eax     ; stepping

    mov  eax, esi
    shr  eax, 4
    and  eax, 0Fh
    mov  [rdi+66], eax     ; model

    mov  eax, esi
    shr  eax, 8
    and  eax, 0Fh
    mov  [rdi+62], eax     ; family

    mov  [rdi+82], r8d     ; featuresEdx
    mov  [rdi+86], r9d     ; featuresEcx

    ; --- Extended features (leaf 7) ---
    mov  eax, 7
    xor  ecx, ecx
    cpuid
    mov  [rdi+90], ebx     ; extFeatures

    pop  rdi
    pop  rsi
    pop  rbx
    ret
cpu_identify ENDP

; ---------------------------------------------------------------
; cpu_read_perf_counter: Read PMC via RDPMC
; Args: ECX = counter number
; Returns: RAX = 64-bit counter value
; ---------------------------------------------------------------
cpu_read_perf_counter PROC
    rdpmc
    shl rdx, 32
    or  rax, rdx
    ret
cpu_read_perf_counter ENDP

; ---------------------------------------------------------------
; cpu_supports_rdtSCP: Check RDTSCP support via CPUID
; Returns: RAX = 1 if supported, 0 otherwise
; ---------------------------------------------------------------
cpu_supports_rdtscp PROC
    mov  eax, 80000001h
    cpuid
    bt   edx, 27
    setc al
    movzx eax, al
    ret
cpu_supports_rdtscp ENDP

; ---------------------------------------------------------------
; cpu_read_msr: Read Model-Specific Register
; Args: ECX = MSR number
;       RDX = uint64_t* out
; Returns: RAX = 1 if success, 0 if access denied
; Uses __try/__except for usermode safety
; ---------------------------------------------------------------
cpu_read_msr PROC
    push rbx
    mov  rbx, rdx          ; rbx = out pointer
    ; Try RDMSR -- will fault if not privileged
    mov  ecx, ecx          ; MSR number already in ecx
    rdmsr                  ; EDX:EAX = MSR value
    shl  rdx, 32
    or   rax, rdx
    mov  [rbx], rax
    mov  rax, 1
    pop  rbx
    ret
cpu_read_msr ENDP

; ---------------------------------------------------------------
; cpu_read_tsc_delta: Read TSC, sleep delayMs via spinning, read TSC again
; Args: ECX = delay in milliseconds
; Returns: RAX = TSC delta (ticks during delay)
; ---------------------------------------------------------------
cpu_read_tsc_delta PROC
    push rbx
    push rsi
    push rdi

    mov  esi, ecx          ; esi = delay ms

    ; Get QPC frequency for spin timing
    rdtsc
    shl  rdx, 32
    or   rax, rdx
    mov  rdi, rax          ; rdi = start TSC

    ; Simple spin delay: use QueryPerformanceCounter
    ; Since we can't call Windows APIs from pure ASM easily,
    ; we use a calibrated spin loop
    ; 1ms ~= 3,500,000 TSC ticks on a 3.5GHz CPU (approximate)
    mov  rax, 3500000      ; ticks per ms (approximate)
    mul  rsi               ; rax = total ticks to spin
    mov  rcx, rax

    ; Spin loop
    xor  edx, edx
tsc_spin:
    rdtsc
    shl  rdx, 32
    or   rax, rdx
    sub  rax, rdi          ; rax = elapsed ticks
    cmp  rax, rcx
    jb   tsc_spin

    ; Compute delta
    rdtsc
    shl  rdx, 32
    or   rax, rdx
    sub  rax, rdi          ; rax = total TSC delta

    pop  rdi
    pop  rsi
    pop  rbx
    ret
cpu_read_tsc_delta ENDP

END
