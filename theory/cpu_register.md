CPU registers = tiny storage inside the CPU itself

Think layers:

Disk → very slow
RAM → fast
Registers → fastest

What they are:

Built directly into the processor (e.g. CPU)
Hold very small data (usually 64 bits = 8 bytes)
Access time ≈ 1 clock cycle

Why they exist:

CPU does calculations only on registers
Data from RAM is first loaded → register → processed
Example flow:

Load x from RAM → register
Add 1 (inside CPU)
Store back to RAM

Why faster than RAM:

No distance, no bus → physically inside CPU
No waiting → immediate access
================
volatile keyword:
Compiler may keep a variable only in a register
So RAM is not updated/read every time
volatile forces: always go to RAM, not just register

Short:

Registers = CPU's working table
RAM = storage room