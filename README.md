# Project Symbiote (Hobby, Active development)
Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine,  
developed assisted with DeepSeek & ClaudeAI, using an [older kernel template](https://github.com/TheBarret/Kernel)

<img width="1024" height="559" alt="logo" src="https://github.com/user-attachments/assets/808c960e-6db9-469d-93ed-fd51f01e92cf" />

---

## Operational

<img width="1290" height="850" alt="greeter" src="https://github.com/user-attachments/assets/db988298-4826-4ba0-9833-684502993989" />  

## Changelog

- `lib/mem.h`, `lib/mem.c`: added `memchr`, `explicit_bzero`, and named align/bit helpers;  
  (`is_power_of_two`, `align_up`, `align_down`, `ptr_align_up`, `ptr_align_down`, `bit_test`, `bit_set`, `bit_clear`).  
- `lib/string.h`, `lib/string.c`: added `strrchr`, `strcat`, `strncat`, `strspn`,  
  `strcspn`, `strpbrk`, `strstr`, `strtoull`, `strtoll`, and the strict wrapper `kstrtoull`.  
- `lib/string.c`: rewrote `strtoll`'s overflow check as two explicit branches.  
- `core/cmd.c`: `parse_num` now calls `kstrtoull`; `parse_spec` uses `strspn`; self-check now rejects whitespace in command names.
- `core/kprintf.h`, `core/kprintf.c`: hardened rewrite: exposed `kemit_fn`/`kvformat`,  
   added precision (`.N`/`.*`), added `%o` and `%b`, made `h` truncate to 16 bits, sized the digit buffer for base 2,  
   replaced the long parameter list with `struct field`.  
- `core/heap.c`: `struct chunk` records its own size; `heapcheck` uses it instead of assuming `HEAP_CHUNK_SIZE`.  
- `core/heap.c`: `kmalloc` and `krealloc` now reject sizes that would overflow the alignment and header arithmetic.  
- `core/pmm.c`: bitmap placement in Pass 2 now aligns the candidate base before checking size against the entry's end.  
- `ext/selftest.c`: fixed the `pmm_contiguous: ok` label to `pmm_contig: ok`.  

**Boot**  
Booting from an ISO under BIOS and UEFI, verified against the Limine protocol revision at every startup.  
Memory map and HHDM offset queried from the bootloader.  
`ACPI, RSDP` and boot modules queried via the same request mechanism.  

**Consoles**  
Two independent output paths, serial `(COM1, 115200 8N1)` and framebuffer (via `flanterm`), fed by one formatter.  
Every byte written anywhere is also kept in a `16 KiB` ring buffer,  
so a console that comes up late receives the entire boot log at registration.  
`console_putc` provides a character-at-a-time path for interactive echo without going through the formatter.

**Formatter**  
`kprintf` and `ksnprintf`, with the standard set of integer, string, and pointer conversions, `-`/`0` flags,  
width (literal or `*`), and `l`/`ll`/`z` length modifiers. Compiler format-string checking on every call.  

**Keyboard**  
Interrupt-driven PS/2 driver on IRQ1, controller self-test, scancode-set configuration via controller translation,  
extended-key handling, modifier tracking, and device separation (keyboard vs mouse bytes via the `AUXBUF` status bit).  
Raw scancodes are pushed into a ring buffer by the IRQ handler;  
decode runs in thread context, so the interrupt path never prints or allocates.  
`kbd_getchar` blocks with `sti; hlt`, waking only when a key arrives.  

**Shell**  
Interactive line editor with history-free backspace, in-place tokenizer,  
and a command dispatcher driven entirely by a link-time registry.  
The shell itself is a dumb parser; every command is an extension.  
Only `help` and `halt` remain built-in, on the principle that the shell should always know how to describe itself,  
and always know how to stop.  

**Memory**
- **Physical**: bitmap page allocator initialized from the bootloader's memory map,  
  with a self-test that round-trips several thousand frames.  
  Hands out 4 KiB frames and contiguous runs. Bitmap placed in usable RAM via HHDM, not statically reserved.  

- **Virtual**: 4-level page tables cloned from Limine, CR3 taken over, W^X applied to kernel sections,  
  and a small API for creating and modifying mappings in the current address space.  
  `VMM_NOCACHE` supported for `MMIO`.  

- **Heap**: chunked free list with block headers and immediate coalescing.  
  `kmalloc`, `kfree`, `kzalloc`, `krealloc`. Grows by 256 KiB chunks from the PMM.  
  Debug build (`-DSYM_MEMDEBUG`) adds header magic, poison on free and alloc, and owner tagging.  

**Interrupts**  

<img width="512" height="91" alt="Interrupts" src="https://github.com/user-attachments/assets/6f91f1f3-c2be-4347-bc3d-4965a67c85fc" />  

GDT with a TSS and IST stacks for the double-fault vector.  
IDT with all `256` gates filled: exceptions `0–31`, IRQs `32–47`, default handler beyond that.  
NASM-free stubs in `isr_stubs.S`, compiled by the same GCC invocation as the rest of the kernel.  

PIC remap to vectors 0x20–0x2F, mask-on-register, per-line spurious handling.  
LAPIC enabled and LINT0 wired to ExtINT so PIC interrupts reach the CPU.  

PIT at `1000 Hz` driving a tick counter, `sleep_ms` and `uptime_ms`.  
Exception handler prints the vector name, error code, registers, and backtrace through the existing panic path.  

**Extensions**  

Link-time discovery via a dedicated linker section, priority-ordered initialization, 
failure-tolerant, removable from the image by name.  
A parallel command registry (`SYM_COMMAND`, section `.symbiote_cmd`) does the same for shell commands,  
with declarative parameter specs, centralized argument validation, and a boot-time self-check  
that catches duplicate names and malformed specs.  

**Forth/Brainfuck**  
Small stack or tape-based (arithmetic) evaluators, usable interactively (`forth <expr>`, `bf <source>`), 
or as a boot-time script loaded by Limine as a module.  

**Diagnostics**  
Panic handler with register dump, frame-pointer backtrace, exception vector decoding,  
and active-extension / active-command reporting.  
`klog` boot log with a fixed-width tag column and per-subsystem coloring.  

<img width="812" height="573" alt="Faults" src="https://github.com/user-attachments/assets/e1c903b9-dcb8-449e-bad3-2656bc9b6f97" />

## Virtual Filesystem

Currently developing a `ramfs` that is flat and has the basic toolchain (create, read, write, and delete for files and folders).  

<img width="1290" height="850" alt="vfs" src="https://github.com/user-attachments/assets/029a2e37-4383-41cc-bc55-0e6bfdb9114a" />  

*early filesystem testing*  

## klog + TSC Counter

<img width="1290" height="850" alt="tsc" src="https://github.com/user-attachments/assets/231bb479-ee01-4b8d-bbbb-1976283d6852" />  

*early tsc logging*  


---

## Not operational (yet)

- No filesystem *(active development)*.
- No block device driver, no PCI enumeration.
- No processes, no user mode, no syscalls.
- No SMP. Single-core assumption throughout, documented where it matters.
- No swap, no demand paging, no copy-on-write.
- No APIC timer (PIT only), no IOAPIC, no MSI.
- No hardware inspection beyond the memory map and framebuffer (planned: CPUID, ACPI, PCI, SMBIOS).

---

## Layout

```
kernel/
  src/
    main.c            boot sequence
    core/             boot, cmd, console, cpu, ext, gdt, heap, idt, isr, keyboard, tui, klog,
                      kprintf, panic, pic, pmm, serial, shell, timer, version, vmm
    lib/              mem, string
    ext/              fbcon, hello, cmd_heap, cmd_mem, cmd_sys, forth, selftest
    isr_stubs.S       interrupt entry stubs (GNU as)
  linker-scripts/     x86_64 memory layout
  GNUmakefile         kernel build
GNUmakefile           ISO assembly, QEMU targets
limine.conf           bootloader config
```

---

## Building

Requires a recent GCC or Clang, GNU Make, and `xorriso`.  
First build fetches Limine and the freestanding headers, then compiles the kernel and assembles a hybrid BIOS/UEFI ISO.

```
  make                # build everything, produce symbiote-x86_64.iso
  make run-bios       # boot under QEMU, BIOS path
  make run            # boot under QEMU, UEFI path
  make run-serial     # boot headless, serial console to the terminal
  make -C kernel size # report kernel size
```

Build with `CPPFLAGS=-DSYM_MEMDEBUG` to enable heap header magic, poison, and owner tagging.  
Build with `EXTENSIONS="hello"` for a headless image with only the smoke-test extension.

## Third-party License

This software incorporates components from third-party projects, detailed below:  
```  
  -------------------------------------------------------------------------------
  1. Limine Bootloader (https://github.com/limine-bootloader/limine)
  -------------------------------------------------------------------------------
  Copyright (c) 2019-2026 Mintsuki and contributors
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  
  1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.
  
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING FROM, OUT OF OR IN
  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  
  -------------------------------------------------------------------------------
  2. flanterm (https://github.com/mintsuki/flanterm)
  -------------------------------------------------------------------------------
  Copyright (c) 2022-2026 Mintsuki and contributors
  
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  
  1. Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.
  
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING FROM, OUT OF OR IN
  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---
