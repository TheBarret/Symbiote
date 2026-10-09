# Project Symbiote (Hobby, Active development)

Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine,  
developed with assistance from DeepSeek and ClaudeAI, using an [older kernel template](https://github.com/TheBarret/Kernel) as a starting base.  

<img width="1024" height="559" alt="logo" src="https://github.com/user-attachments/assets/808c960e-6db9-469d-93ed-fd51f01e92cf" />  

---

## Screenshot

<img width="1290" height="850" alt="0.1.0" src="https://github.com/user-attachments/assets/8d61b571-d70e-478a-9461-3366b913cab4" />


---

## Operational

**Boot**  
Booting from an ISO under BIOS and UEFI, verified against the Limine protocol revision at every startup.  
Memory map and HHDM offset queried from the bootloader. ACPI, RSDP, and boot modules queried via the same request mechanism.  

**Consoles**  
Two independent output paths, serial (`COM1, 115200 8N1`) and framebuffer (via `flanterm`),  
fed by one formatter, every byte written anywhere is also kept in a 16 KiB ring buffer,  
so a console that comes up late receives the entire boot log at registration.  
`console_putc` provides a character-at-a-time path for interactive echo without going through the formatter.  

**Formatter**  
`kprintf` and `ksnprintf`, with integer, string, and pointer conversions, `-`/`0` flags,  
width (literal or `*`), precision (`.N` or `.*`), and `l`/`ll`/`z` length modifiers. `h` truncates to 16 bits.   
Compiler format-string checking on every call.

**Keyboard**  
Interrupt-driven PS/2 driver on IRQ1, controller self-test, scancode-set configuration via controller translation,  
extended-key handling, modifier tracking, and device separation (keyboard vs mouse bytes via the `AUXBUF` status bit).  
Raw scancodes are pushed into a ring buffer by the IRQ handler; decode runs in thread context, so the interrupt path never prints or allocates.  
`kbd_getchar` blocks with `sti; hlt`, waking only when a key arrives.  

**Shell**  
Interactive line editor with history-free backspace, in-place tokenizer, and a command dispatcher driven entirely by a link-time registry.  
The shell itself is a dumb parser; every command is an extension. Only `help` and `halt` remain built-in.  

<img width="544" height="464" alt="help" src="https://github.com/user-attachments/assets/c59d2202-a3a2-4184-9d73-cd24466d3d0e" />  

## Memory

- **Physical**: bitmap page allocator initialized from the bootloader's memory map.
  Hands out 4 KiB frames and contiguous runs. Bitmap placed in usable RAM via HHDM, not statically reserved.  

- **Virtual**: 4-level page tables cloned from Limine, CR3 taken over, W^X applied to kernel sections,
  and a small API for creating and modifying mappings in the current address space. `VMM_NOCACHE` supported for MMIO.  

- **Heap**: chunked free list with block headers and immediate coalescing. `kmalloc`, `kfree`, `kzalloc`, `krealloc`.
  Grows by 256 KiB chunks from the PMM. Debug build (`-DSYM_MEMDEBUG`) adds header magic, poison on free and alloc, and owner tagging.  

  <img width="819" height="627" alt="memory" src="https://github.com/user-attachments/assets/b881a85c-c24a-4c54-bc97-5bf24ff693b7" />

## Filesystem

In-memory `ramfs` under a thin VFS layer, node model with path normalization (`.`, `..`),  
API: `vfs_open`/`close`/`read`/`write`/`lseek`, `vfs_mkdir`/`unlink`/`stat`/`readdir` with a single global spinlock with IRQ-save semantics.  

<img width="401" height="214" alt="filesystem" src="https://github.com/user-attachments/assets/36113f79-2f7c-4140-b344-a0c50b0bf234" />  


## Interrupts

GDT with a TSS and IST stacks for the double-fault vector, IDT with all 256 gates filled:  
exceptions `0–31`, IRQs `32–47`, default handler beyond that.  
NASM-free stubs in `isr_stubs.S`, compiled by the same GCC invocation as the rest of the kernel.  

PIC remap to vectors `0x20–0x2F`, mask-on-register, per-line spurious handling.  
LAPIC enabled and LINT0 wired to ExtINT so PIC interrupts reach the CPU.  
PIT at `1000 Hz` driving a tick counter, timer_sleep_ms and uptime_ms.  
Exception handler prints the vector name, error code, registers, and backtrace through the existing panic path.  

<img width="512" height="91" alt="Interrupts" src="https://github.com/user-attachments/assets/6f91f1f3-c2be-4347-bc3d-4965a67c85fc" />

## Extensions

Link-time discovery via a dedicated linker section, priority-ordered initialization, failure-tolerant removable by name.  
A parallel command registry (`SYM_COMMAND`, section `.symbiote_cmd`) does the same for shell commands,  
with declarative parameter specs, centralized argument validation, and a boot-time self-check that catches duplicate names and malformed specs.  

<img width="643" height="219" alt="toolkits" src="https://github.com/user-attachments/assets/76a878a2-1a36-4e68-a941-0dea270d3d28" />

## Diagnostics

Supports a panic handler with register dump, frame-pointer backtrace, exception vector decoding, and basic reporting.  
The `klog` is primitive log organizer with a fixed-width tag column, per-subsystem coloring, and an `rdtsc`-based timestamp prefix.  
*(microsecond deltas after CPUID calibration, default value as fallback otherwise)*    

## Text-based User Interface - TUI Engine

`core/tui` is a small terminal UI core: two cell grids, rect-based drawing, differential flush.  
A view is a draw callback and a key callback, example app is the `explorer` extension, a single-panel VFS browser.  

<img width="744" height="438" alt="TUI" src="https://github.com/user-attachments/assets/5cea79c7-3963-43c5-a505-893b60ab8e94" />

---

## Not operational (yet)

- No block device driver, no PCI enumeration.
- No on-disk filesystem.
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
                      kprintf, panic, pic, pmm, serial, shell, timer, version, vmm, vfs
    lib/              mem, string
    ext/              fbcon, memory, system, vfs, explorer
    isr_stubs.S       interrupt entry stubs (GNU as)
  linker-scripts/     x86_64 memory layout
  GNUmakefile         kernel build
GNUmakefile           ISO assembly, QEMU targets
limine.conf           bootloader config
```

---

## Building

Requires a recent GCC or Clang, GNU Make, and `xorriso`. First build fetches Limine and the freestanding headers, then compiles the kernel and assembles a hybrid BIOS/UEFI ISO.

```
  make                # build everything, produce symbiote-x86_64.iso
  make run-bios       # boot under QEMU, BIOS path
  make run            # boot under QEMU, UEFI path
  make run-serial     # boot headless, serial console to the terminal
  make -C kernel size # report kernel size
```

Build with `CPPFLAGS=-DSYM_MEMDEBUG` to enable heap header magic, poison, and owner tagging. Build with `EXTENSIONS="hello"` for a headless image with only the smoke-test extension.

---

## Third-party licenses

This software incorporates components from third-party projects, detailed below.

```
------------------------------------------------------------------------------
1. Limine Bootloader (https://github.com/limine-bootloader/limine)
------------------------------------------------------------------------------
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
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

------------------------------------------------------------------------------
2. flanterm (https://github.com/mintsuki/flanterm)
------------------------------------------------------------------------------
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
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---
