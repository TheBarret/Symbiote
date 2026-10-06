# Project Symbiote (Hobby Project)
Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine,  
developed assisted with DeepSeek & ClaudeAI, using an [older kernel template](https://github.com/TheBarret/Kernel)

<img width="1024" height="559" alt="logo" src="https://github.com/user-attachments/assets/808c960e-6db9-469d-93ed-fd51f01e92cf" />

```
    ─────────────────────────── Bootloader ───────────────────────────
    
    [1] FIRMWARE
        (BIOS or UEFI, outside Symbiote)
        - Reads the boot medium
        - Loads Limine
        - Jumps to Limine
    
    [2] LIMINE
        - Reads `limine.conf`
        - Loads /boot/symbiote (the kernel ELF) into memory
        - Sets up long mode, paging, HHDM
        - Scans .limine_requests, answers each request
        - Jumps to the kernel's entry point (`kmain`)
    
    ─────────────────────────── Kernel ───────────────────────────
    
    [3] kmain (src/main.c)
        - boot_protocol_ok()        ── core/boot.c reads limine_base_revision
        - or cpu_halt_forever()
    
        [4] serial_init (core/serial.c)
            - writes to COM1 registers
            - console_register(&serial_ops)  ── core/console.c now has 1 sink
    
        [5] Logger functionality
            - kprintf → format → con_emit → console_write
            - console_write → serial_write
            - ring buffer captures the same bytes for replay later
    
        [6] pmm_init (core/pmm.c)
            - boot_memory_ok() → core/boot.c
            - boot_memmap() → core/boot.c
            - boot_hhdm_offset() → core/boot.c
            - walks map, places bitmap in a usable regions
            - PMM debug verbosity
    
        [7] heap_init (core/heap.c)
            - pmm_alloc_contig(64) → core/pmm.c (one 256 KiB chunk)
            - pmm_phys_to_virt → core/pmm.c → core/boot.c (hhdm offset)
            - memset the chunk's header area (lib/mem.c)
            - Heap debug verbosity
    
        [8] vmm_init (core/vmm.c)
            - read_cr3 (core/cpu.h inline asm)
            - clone_level for each level (recursion)
                - pmm_alloc for each table
                - pmm_phys_to_virt to reach it
            - write_cr3 (core/cpu.h inline asm)
            - apply_kernel_wx (walks .text/.rodata/.data using linker symbols)
            - VMM debug verbosity
    
        [9] ext_init_all (core/ext.c)
            - scan .symbiote_ext markers
            - for each extension, in priority order:
                [9a] fbcon_init (ext/fbcon/fbcon.c)   prio=0
                     - boot_framebuffer() → core/boot.c
                     - flanterm_fb_init(...) module
                     - console_register(&fbcon_ops) → core/console.c
                         → console.c replays the ring buffer
                         → Greeter appears on the framebuffer
                     - returns 0
                [9b] example_init (ext/test.c)          prio=40
                     - Runs no logic (example template)
                     - returns 0
                [9c] shell dispatcher commands are discovered by cmd.c
    
        [10] Extensions are ready to be used
    
        [11] shell_run (core/shell.c) 
            - kbd_init (core/keyboard.c)
                - PS/2 self-test, translation on, aux disable check
                - On error: show error message + cpu_halt_forever
            - cmd_selfcheck (core/cmd.c)
                - scans .symbiote_cmd for duplicate names, malformed specs
            - present 'help' information
            - loop:
                [11a] prefix "> "
                [11b] read_line (core/shell.c)
                      - kbd_getchar (core/keyboard.c)
                          - kbd_poll
                              - inb (core/cpu.h)
                          - returns a char
                      - console_putc echoes to all sinks
                [11c] tokenize (core/shell.c)
                [11d] run_line (core/shell.c)
                      - if argv[0] == "help": built-in handler in shell.c
                          - cmd_count, cmd_at (core/cmd.c) iterate .symbiote_cmd
                          - kprintf
                      - else: cmd_dispatch (core/cmd.c)
                          [11d-1] cmd_find scans .symbiote_cmd
                          [11d-2] parse_spec parses the command's spec string
                          [11d-3] validates argc against required/optional/rest
                          [11d-4] parse_num for each num argument
                          [11d-5] current = cmd->name
                          [11d-6] cmd->fn(&a), the handler runs
                              e.g. 'heap' handler calls heap_get_stats (core/heap.c)
                                   'pages' handler calls pmm_get_stats (core/pmm.c)
                                   'selftest' runs tests that call pmm/heap/vmm
                          [11d-7] current = NULL
                          [11d-8] prints exit code if non-zero
                [11e] loop back to 11a
    
    ─────────────────── Loop until exit ───────────────────
```

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
*Shell.c has migrated all its command structures into extension based calling convention, see ref: `Symbiote/kernel/src/core/cmd.h`*  

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
