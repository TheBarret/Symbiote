# Project Symbiote (Hobby Project)
Symbiote is a bootloader-agnostic x86_64 kernel that boots via Limine

<img width="1024" height="559" alt="image" src="https://github.com/user-attachments/assets/359926a7-42e1-4ac2-961f-b4eab59bd301" />


# Changelog and Activity

**Boot and protocol**  
- Limine boots the kernel from an ISO under both BIOS and UEFI. Hybrid ISO assembled by the master `Makefile`.
- `boot_protocol_ok()` verifies Limine's revision against `LIMINE_BASE_REVISION(6)`. On mismatch, kernel halts silently. (`core/boot.c`)
- `boot_framebuffer()` returns the first Limine framebuffer or NULL. (`core/boot.c`)

**Hardware primitives**  
- Port I/O: `outb`, `inb`, `inw`, `insw`, `outsw`. (`core/cpu.h`)
- Interrupt control: `cpu_cli`, `cpu_sti`. (`core/cpu.h`)
- `cpu_halt_forever()`: disables interrupts, halts forever. (`core/cpu.h`)

**Console system**  
- A fixed table of up to 4 `console_ops` sinks. Registration is append-only; overflow is silent. (`core/console.c`)
- Every `console_write` forwards raw bytes to every registered sink and appends them to a 16 KiB ring buffer. (`core/console.c`)
- Late-attaching consoles receive the whole boot log (or the last 16 KiB of it) at registration time. (`core/console.c`)

**Serial console**  
- COM1 at 0x3F8 brought up at 115200 8N1, FIFOs enabled, interrupts off. (`core/serial.c`)
- `\n` → `\r\n` translation per-sink. (`core/serial.c`)
- `serial_putc` has a bounded spin, so a dead UART cannot hang the kernel. (`core/serial.c`)

**Formatter**  
- `kprintf`, `kvprintf`: formatted output to every console, buffered at 128 bytes per call. (`core/kprintf.c`)
- `ksnprintf`, `kvsnprintf`: same formatter into a caller-supplied buffer, with snprintf truncation contract. (`core/kprintf.c`)
- Supported: `%c %s %d %i %u %x %X %p %%`, flags `-` and `0`, width (number or `*`), length modifiers `l ll z` (all 64-bit). (`core/kprintf.h`)
- NULL `%s` prints `(null)`. Unknown conversions print literally. (`core/kprintf.c`)

**Extension system**  
- Extensions are `static const struct sym_ext` placed in `.symbiote_ext` by the `SYM_EXTENSION` macro. (`core/ext.h`)
- The linker script places `__symbiote_ext_start` and `__symbiote_ext_end` around the section. (`linker-scripts/x86_64.lds`)
- `ext_init_all()` walks priority levels in ascending order using a selection scan (no sort, no mutation). Failures are logged and skipped; boot continues. (`core/ext.c`)
- `ext_current()` returns the name of the extension whose init is running, which the panic handler reads. (`core/ext.c`)
- `ext_count()` returns the number of registered extensions. (`core/ext.c`)
- Priority levels: `CONSOLE=0`, `DRIVER=10`, `FS=20`, `SERVICE=30`, `APPLET=40`. (`core/ext.h`)
- Removing an extension from `EXTENSIONS` removes its code from the image (`--gc-sections`). (kernel `GNUmakefile`)

**Extensions shipped**  
- `fbcon`: framebuffer console via flanterm, phosphor green on near-black, only accepts 32-bit RGB framebuffers. Registers at `EXT_PRIO_CONSOLE`. (`ext/fbcon/fbcon.c`)
- `hello`: one-line smoke test at `EXT_PRIO_APPLET`. Optional compile-time panic via `-DSYM_TEST_PANIC`. (`ext/hello.c`)

**Panic and assertion**  
- `PANIC(...)` and `ASSERT(cond)` macros. (`core/panic.h`)
- `panic_at()` disables interrupts, guards against re-entry, dumps registers, prints the active extension, walks the frame-pointer chain (bounded to 16 frames with alignment and kernel-half checks), and halts. (`core/panic.c`)
- **The panic path is compiled and linked but **has not been exercised** on this machine yet.**

**Memory and string primitives**  
- `memcpy`, `memset`, `memmove`, `memcmp` exist as real symbols (compiler fallback) and are routed to compiler builtins at normal call sites via macros. (`lib/mem.c`, `lib/mem.h`)
- `strlen`, `strcmp`, `strncmp`: standard behavior including the unsigned-char comparison rule in the `str*` functions. (`lib/string.c`, `lib/string.h`)

**Boot sequence**  
- `kmain` verifies protocol → brings up serial → prints version → runs all extensions → prints readiness banner with `ext_count()` → halts. (`src/main.c`)

<img width="1290" height="850" src="https://github.com/user-attachments/assets/f58a6ece-fbc9-41b1-b44b-a0d4cb575248" />  

*early shell testing*

**Version header**  
`src/core/version.h` now holds `SYM_VERSION`, and both `main.c` and `shell.c` include it.   
The duplication that was flagged in the shell pass is gone. One source of truth for the version string.  

**Keyboard driver**   
`src/core/keyboard.h` / `keyboard.c` went through three real fixes:
- The API changed from `char kbd_poll(void)` to `enum kbd_event kbd_poll(char *out)`, with distinct events for "no key", "character", "special key", and "unknown scancode". Modifier events and unrecognized scancodes are no longer conflated with "nothing happened".
- `kbd_init()` now returns `bool`. It runs the PS/2 self-test (`0xAA`), reads the controller configuration byte, sets translation on, clears the disable-clock bit, and re-enables the device. On a machine with no controller, it returns `false` and the shell halts with a message instead of silently blocking on a nonexistent keyboard.
- Scancode set is now handled by the controller's translation, not by asking the device to switch to set 1. That was the fix for the "random characters" symptom: the device was emitting set 2 and the tables were set 1. Turning translation on makes the controller do the conversion, which is what every real PS/2 driver does.
- The AUXBUF bit (0x20 of port 0x64) is now checked in `kbd_poll`. Bytes from the mouse are consumed and dropped instead of being decoded as scancodes. That was the fix for "moving the mouse produces characters" and "alt-tab scrambles the keyboard".

**Shell**  
`src/core/shell.c` runs at `kmain`'s end, replaces the halt. Seven commands: `help`, `echo`, `clear`, `version`, `exts`, `panic`, `halt`.  
Line editor handles backspace. Tokenizer splits on spaces and tabs in place. Nonzero exit codes from commands are reported.  
The prompt loop never returns, which is why the shell is core and not an extension.  

**Bugs fixed:**  
1. Random characters from set-1/set-2 mismatch → fixed by enabling controller translation.
2. Characters appearing when the mouse moved → fixed by checking AUXBUF before reading from 0x60.

