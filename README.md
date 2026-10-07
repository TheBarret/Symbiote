# Project Symbiote (Hobby Project)
Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine,  
developed assisted with DeepSeek & ClaudeAI, using an [older kernel template](https://github.com/TheBarret/Kernel)

<img width="1024" height="559" alt="logo" src="https://github.com/user-attachments/assets/808c960e-6db9-469d-93ed-fd51f01e92cf" />

---

## Operational

<img width="1290" height="850" alt="greeter" src="https://github.com/user-attachments/assets/feada229-40fb-4ee4-8b1a-f7cc6de0c827" />


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
    IDT with all 256 gates filled: exceptions 0–31, IRQs 32–47, default handler beyond that.  
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

**Forth**  
Small stack-based arithmetic evaluator, usable interactively (`forth <expr>`), 
or as a boot-time script loaded by Limine as a module.  
Used as an ALU, not as a system language: it takes numbers in, produces numbers out,  
and knows nothing about strings, files, or the kernel outside of a small set of registered words.  

<img width="546" height="263" alt="Extensions" src="https://github.com/user-attachments/assets/24c1b223-cb8a-4a3b-9b6c-0b6db9fdc4a6" />  

**Diagnostics**  
Panic handler with register dump, frame-pointer backtrace, exception vector decoding,  
and active-extension / active-command reporting.  
`klog` boot log with a fixed-width tag column and per-subsystem coloring.  

---

## Not operational

- No block device driver, no PCI enumeration.
- No filesystem (planned: read-only, loaded as a Limine module).
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
    core/             boot, cmd, console, cpu, ext, gdt, heap, idt, isr, keyboard, klog,
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
