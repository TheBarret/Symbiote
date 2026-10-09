# Project Symbiote (Hobby, Active development)

Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine,  
developed with assistance from DeepSeek and ClaudeAI, using an [older kernel template](https://github.com/TheBarret/Kernel) as a starting base.  

<img width="1024" height="559" alt="logo" src="https://github.com/user-attachments/assets/808c960e-6db9-469d-93ed-fd51f01e92cf" />  

---

## Screenshot

<img width="1290" height="850" src="https://github.com/user-attachments/assets/231bb479-ee01-4b8d-bbbb-1976283d6852" />  

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

**Memory**  
- **Physical**: bitmap page allocator initialized from the bootloader's memory map.
  Hands out 4 KiB frames and contiguous runs. Bitmap placed in usable RAM via HHDM, not statically reserved.  

- **Virtual**: 4-level page tables cloned from Limine, CR3 taken over, W^X applied to kernel sections,
  and a small API for creating and modifying mappings in the current address space. `VMM_NOCACHE` supported for MMIO.  

- **Heap**: chunked free list with block headers and immediate coalescing. `kmalloc`, `kfree`, `kzalloc`, `krealloc`.
  Grows by 256 KiB chunks from the PMM. Debug build (`-DSYM_MEMDEBUG`) adds header magic, poison on free and alloc, and owner tagging.  

- **Filesystem**: in-memory `ramfs` under a thin VFS layer. Node model with path normalization (`.`, `..`),
  `vfs_open`/`close`/`read`/`write`/`lseek`, `vfs_mkdir`/`unlink`/`stat`/`readdir`. Single global spinlock with IRQ-save semantics.  

**Interrupts**

<img width="512" height="91" alt="Interrupts" src="https://github.com/user-attachments/assets/6f91f1f3-c2be-4347-bc3d-4965a67c85fc" />

    GDT with a TSS and IST stacks for the double-fault vector. IDT with all 256 gates filled:  
    exceptions 0–31, IRQs 32–47, default handler beyond that.  
    NASM-free stubs in isr_stubs.S, compiled by the same GCC invocation as the rest of the kernel.  
    
    PIC remap to vectors 0x20–0x2F, mask-on-register, per-line spurious handling.  
    LAPIC enabled and LINT0 wired to ExtINT so PIC interrupts reach the CPU.  
    PIT at 1000 Hz driving a tick counter, timer_sleep_ms and uptime_ms.  
    Exception handler prints the vector name, error code, registers, and backtrace through the existing panic path.  

**Extensions**  

Link-time discovery via a dedicated linker section, priority-ordered initialization, failure-tolerant removable by name.  
A parallel command registry (`SYM_COMMAND`, section `.symbiote_cmd`) does the same for shell commands,  
with declarative parameter specs, centralized argument validation, and a boot-time self-check that catches duplicate names and malformed specs.  

**Diagnostics**  

Supports a panic handler with register dump, frame-pointer backtrace, exception vector decoding, and basic reporting.  
The `klog` is primitive log organizer with a fixed-width tag column, per-subsystem coloring, and an `rdtsc`-based timestamp prefix.  
*(microsecond deltas after CPUID calibration, 1 GHz fallback otherwise)*    

**Text-based User Interface - TUI Engine**  

<img width="1290" height="850" alt="TUI" src="https://github.com/user-attachments/assets/74931765-08a2-4b49-aa24-5a7fe35ec625" />


`core/tui` is a small terminal UI core: two cell grids, rect-based drawing, differential flush.  
A view is a draw callback and a key callback, example app is the `explorer` extension, a single-panel VFS browser.  

---

## Changelog

- `lib/mem.h`, `lib/mem.c`: added `memchr`, `explicit_bzero`, and named align/bit helpers (`is_power_of_two`, `align_up`, `align_down`, `ptr_align_up`, `ptr_align_down`, `bit_test`, `bit_set`, `bit_clear`).
- `lib/string.h`, `lib/string.c`: added `strrchr`, `strcat`, `strncat`, `strspn`, `strcspn`, `strpbrk`, `strstr`, `strtoull`, `strtoll`, and the strict wrapper `kstrtoull`.
- `lib/string.c`: rewrote `strtoll`'s overflow check as two explicit branches.
- `core/cmd.c`: `parse_num` now calls `kstrtoull`; `parse_spec` uses `strspn`; self-check now rejects whitespace in command names.
- `core/kprintf.h`, `core/kprintf.c`: hardened rewrite. Exposed `kemit_fn`/`kvformat`, added precision (`.N`/`.*`), added `%o` and `%b`, made `h` truncate to 16 bits, sized the digit buffer for base 2, replaced the long parameter list with `struct field`.
- `core/heap.c`: `struct chunk` records its own size; `heapcheck` uses it instead of assuming `HEAP_CHUNK_SIZE`.
- `core/heap.c`: `kmalloc` and `krealloc` now reject sizes that would overflow the alignment and header arithmetic.
- `core/pmm.c`: bitmap placement in Pass 2 now aligns the candidate base before checking size against the entry's end.
- `core/klog.c`: fixed a missing closing brace in `klog_init` that collapsed the function body. Added timestamp prefix: `rdtsc`-based delta before the PIT is up, `[+ms.us]` after, with 1 GHz fallback.
- `core/cpu.h`, `core/cpu.c`: added `rdtsc()` and `cpu_tsc_hz()` (CPUID 0x15, family/model table for the `ECX == 0` case).
- `core/vfs.h`, `core/vfs.c`: new VFS layer. Node model, path normalization with `.` and `..`, `vfs_open`/`close`/`read`/`write`/`lseek`, `vfs_mkdir`/`unlink`/`stat`/`readdir`. Single global spinlock with IRQ-save semantics. `vfs_open` allocates the handle before linking the node. `vfs_lseek` does all offset math in `uint64_t` with explicit sign handling.
- `core/tui.h`, `core/tui.c`: small TUI core. Two cell grids, rect drawing (`tui_put`/`tui_text`/`tui_fill`/`tui_box`), differential flush, `struct tui_view` with draw/key callbacks.
- `core/keyboard.c`, `core/keyboard.h`: added `kbd_wait()`, which blocks on a decodable key event via `cpu_sti_hlt`.
- `src/ext/fs/`: VFS shell commands `ls`, `cat`, `mkdir`, `rm`, `write`, plus an `fs` extension init.
- `src/ext/explorer/explorer.c`: single-panel VFS browser. Arrow keys move, Enter descends, Backspace ascends, `q` quits. Scrolling list, highlighted selection, framed layout.
- `selftest.c`: removed.
- `boot/locals.fs`, `boot/locals.bf`, and various test files removed.
- `kernel/GNUmakefile`, top-level `GNUmakefile`: `EXTENSIONS` list revised; `forth` and `bf` shelved.
- `core/tui.h`: added a screen-ownership invariant comment. `tui_begin`/`tui_end` bracket a session; between them the TUI owns the screen, before and after the console does, with default SGR and a visible cursor.
- `core/tui.h`: documented that `TUI_ATTR(fg, bg)` backgrounds are honoured by `tui_flush`; previously the byte was accepted and silently dropped.
- `core/tui.h`, `core/tui.c`: added `tui_run_view`, which brackets `tui_run` with `tui_begin`/`tui_end`. Prevents a view that returns early, or a draw callback that panics, from stranding the screen in TUI mode with the cursor hidden.
- `core/tui.c`: `tui_begin` now emits `\x1b[0m` before clearing. A prior command's SGR no longer leaks into the TUI's first frame.
- `core/tui.c`: `tui_end` now resets SGR, clears the screen, and homes the cursor, in addition to showing it. The previous version relied on `tui_begin`'s clear and used a trailing `\n` as a stand-in for home.
- `core/tui.c`: `tui_flush` now emits both nibbles of the attribute byte. Backgrounds were advertised by the macro and ignored by the emitter.
- `core/tui.c`: `tui_flush` drains to `console_write` in chunks instead of accumulating into a fixed 8 KiB buffer. The old buffer filled silently on full-screen changes, dropping cells and leaving the front grid out of sync with the back grid.
- `core/tui.c`: `tui_flush` now advances `last_y` alongside `last_x` after each character write. `last_y` was previously set only inside the cursor-move branch, which happened to work but was not an invariant.
- `core/tui.c`: moved the SGR tables to file scope and deleted the dead `emit_attr` helper.
- `core/tui.c`: replaced hand-counted escape-sequence lengths with `sizeof - 1` over file-scope string constants.
- `core/pic.h`: documented that `pic_eoi` must not be called after `pic_irq_is_spurious` returns true; that function already sent the correct (or no) EOI, and a second one desyncs the in-service register.
- `core/pic.c`: `pic_eoi` now bounds-checks `irq`, matching `pic_mask` and `pic_unmask`. An out-of-range call previously sent an EOI with no corresponding interrupt.
- `core/pic.c`: `pic_mask` and `pic_unmask` now bracket the read-modify-write of `irq_mask_bits` and the two `write_masks` `outb`s in a `cpu_irq_save`/`cpu_irq_restore` region. Removes a lost-update race with nested or future SMP callers.
- `core/pic.c`: `lapic_virtual_wire` now logs when the LAPIC MMIO map fails. Silent failure on the boot path meant IRQs never arrived and the only clue was the absence of activity.
- `core/cpu.h`: port I/O (`outb`, `inb`, `inw`, `outl`, `inl`) now carries a `"memory"` clobber. Port accesses can have memory side effects and must not be reordered relative to memory.
- `core/cpu.h`: `cpu_hlt` now carries a `"memory"` clobber. It is a synchronisation point; code after the halt may depend on data the wakeup interrupt wrote.
- `core/cpu.h`: `read_cr2`, `read_cr3`, `write_cr3`, and `invlpg` now carry a `"memory"` clobber. A CR3 write remaps the whole address space; an `invlpg` invalidates a TLB entry. Stores must not hoist across either.
- `core/cpu.h`: added `outw` for symmetry with `inw`.
- `core/cpu.h`: moved `outl` and `inl` up into the Port I/O section. They were filed under "PCI handlers" and read as PCI-specific when they are not.


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
