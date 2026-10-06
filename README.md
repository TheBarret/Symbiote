# Project Symbiote (Hobby Project)
Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine,  
developed assisted with DeepSeek & ClaudeAI, using an [older kernel template](https://github.com/TheBarret/Kernel)

<img width="1024" height="559" alt="logo" src="https://github.com/user-attachments/assets/808c960e-6db9-469d-93ed-fd51f01e92cf" />

---

## Operational

<img width="1290" height="850" alt="image" src="https://github.com/user-attachments/assets/fdbdc72d-e714-4956-976c-ab3ddd40b574" />  

**Boot**  
Booting from an ISO under BIOS and UEFI, verified against the Limine protocol revision at every startup.  
Memory map and HHDM offset are queried from the bootloader.  

**Consoles**  
Two independent output paths, serial (COM1, 115200 8N1) and framebuffer (via flanterm), fed by one formatter.  
Every byte written anywhere is also kept in a 16 KiB ring buffer,  
so a console that comes up late receives the entire boot log at registration.  

**Formatter**  
`kprintf` and `ksnprintf`, with the standard set of integer, string, and pointer conversions, `-`/`0` flags,  
width (literal or `*`), and `l`/`ll`/`z` length modifiers. Compiler format-string checking on every call.  

**Keyboard and shell**  
Polled PS/2 driver with controller self-test, scancode-set configuration via controller translation,  
extended-key handling, modifier tracking, and device separation (keyboard vs mouse bytes).  
Interactive shell with a line editor and command dispatch.  

**Memory**
- **Physical**: 
  bitmap page allocator initialized from the bootloader's memory map, with a self-test that round-trips several thousand frames. Hands out 4 KiB frames and contiguous runs.
- **Virtual**:  
  4-level page tables cloned from Limine, CR3 taken over, W^X applied to kernel sections,  
  and a small API for creating and modifying mappings in the current address space.  
- **Heap**:  
  chunked free list with block headers and coalescing. `kmalloc`, `kfree`, `kzalloc`, `krealloc`. 
  Debug build (`-DSYM_MEMDEBUG`) adds header magic, poison on free and alloc, and owner tagging.  

**Extensions**  
Link-time discovery via a dedicated linker section, priority-ordered initialization, failure-tolerant,  
removable from the image by name.  
*Shell.c has migrated all its command structures into extension based calling convention, see ref: `/src/kernel/cmd.h`*  

**Diagnostics**  
Panic handler with register dump, frame-pointer backtrace, and active-extension reporting.   
`klog` boot log with a fixed-width tag column and per-subsystem coloring.  

<img width="769" height="546" alt="diagnostics" src="https://github.com/user-attachments/assets/5251c0e8-c204-44c1-946d-d1072d9c17e9" />


---

## Not operational

- No interrupts. No IDT, no GDT setup, no timer, no APIC.
- No block device driver, no PCI enumeration.
- No filesystem (in progress: read-only, loaded as a Limine module).
- No processes, no user mode, no syscalls.
- No SMP. Single-core assumption throughout, documented where it matters.
- No swap, no demand paging, no copy-on-write.

---

## Layout

```
kernel/
  src/
    main.c            boot sequence
    core/             boot, cpu, console, ext, klog, kprintf, panic, pmm, vmm, heap, keyboard, shell, serial, version
    lib/              mem, string
    ext/              fbcon, hello, test
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

To build without the framebuffer console (serial-only, smaller image):
```
  make EXTENSIONS="hello"
```
