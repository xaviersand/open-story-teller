# Micro VM

The stories created through the official Story Editor using graphical nodes are compiled into a binary executable. This binary is then interpreted by a micro virtual machine embedded in the OpenStory firmware.

Here is a description of that VM.

# Registers

| name  | number | type                             | call-preserved |
|-------|--------|----------------------------------|-----------|
| r0-r9 | 0-9    | general-purpose                  | N         |
| t0-t9 | 10-19  | temporary registers              | Y         |
| pc    | 20     | program counter                  | Y         |
| sp    | 21     | stack pointer                    | Y         |
| ra    | 22     | return address                   | N         |


# Instructions

| Instruction  |   Encoding    | Arguments (bytes)   | Description |   Example  |
|-------       |--------       |-------------        |-----------| -----|
| nop  |     0            |    0    | Do nothing   |  |
| halt  |    1            |    0    | Halt execution   |  |
| syscall  |   2           |   1    |  System call handled by user-registered function. Machine specific. Argument is the system call number, allowing up to 256 system calls. |  `syscall 42` |
| lcons  |  3           |   4    |  Store an immediate value in a register. Can also store a variable address. | `lcons r0, 0xA201d800`, `lcons r2, $DataInRam` |
| mov  |  4           |   2   |  Copy a register in another one. | `mov r0, r2` |
| push  |  5          |   1  |  Push a register onto the stack. | `push r0` |
| pop  |  6          |   1  |  Pop the first element of the stack to a register. | `pop r7` |
| store  |       7    |   3  |  Copy a value from a register to a ram address located in a register with a specified size | `store @r4, r1, 2` ; Copy R1 to address of R4, 2 bytes |
| load |      8     |  3   |  Copy a value from a ram address located in a register to a register, with a specific size. | `load r0, @r3, 1` ; 1 byte |
| add  |   9    |   2  |  sum and store in first reg. | `add r0, r2` |
| sub |    10    |  2   |  subtract and store in first reg. | ` sub r0, r2` |
| mul  |   11  |  2   | multiply and store in first reg . | `mul r0, r2` |
| div  |  12  | 2    |  divide and store in first reg. | `div r0, r2` |
| shiftl  |  13  |  2   |  logical shift left. | `shl r0, r1` |
| shiftr |  14  |    2 |  logical shift right. | `shr r0, r1` |
| ishiftr  |  15   |  2   |  arithmetic shift right (for signed values). | `ishr r0, r1` |
| and |  16   |  2   |  and two registers and store result in the first one. | `and r0, r1` |
| or |  17   |  2   |  or two registers and store result in the first one. | `or r0, r1` |
| xor |  18   |  2   |  xor two registers and store result in the first one. | `xor r0, r1` |
| not |  19   |  1   |  not a register and store result. | ` not r0` |
| call |  20   |  1   |  Set register RA to the next instruction and jump to subroutine, must be a label. | `call .media` |
| ret |  21   |  0   |  return to the address of last callee (RA). | `ret` |
| jump |  22   |  1   |  jump to address (can use label or address). | `jump .my_label` |
| jumpr |  23   |  1   |  jump to address contained in a register. | `jumpr t9` |
| skipz |  24   |  1   |  skip next instruction if zero. | `skipz r0` |
| skipnz |  25   |  1   |  skip next instruction if not zero. | `skipnz r2` |

# Assembler

Basic grammar
------------------
 
Example:

```asm
; ---------------------------- Base node Type: Transition
.mediaEntry0004:
    lcons r0, $fairy
    lcons r1, $la_fee_luminelle
    syscall 1
    lcons r0, .mediaEntry0006
    ret
```

 Global variables
------------------

Example:

```asm
$yourLabel  DC8 "a string", 5, 4, 8  ; Déclaration de constantes
$MaVar      DV32    14      ; Variable en RAM : 14 emplacements de taille 32-bits
```

Grammar:

| Type   | First column  |  Second column    |  Third column   | Next columns  |
|-------  |-------                                 |--------                          |-------------        |-----------| 
| ROM constant  | label statring with dollar sign  |  DC + base size (8, 16, 32)      |   constatant value (integer or string, surrounded by double quotes)   | more  values, separated by a coma  |
| RAM variable  | label statring with dollar sign  |  DV + base size (8, 16, 32)      |  Size of the array (number of elements)   | -  |
