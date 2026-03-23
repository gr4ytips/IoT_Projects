# STM32F411 Secure Boot Lab

A practical firmware integrity and embedded security lab built on the **STM32F411CEU6 Black Pill**.

This project implements a compact cryptographic secure boot chain that validates a signed application image before execution. It is designed as both a working embedded security build and a reusable research platform for later hardening, reverse engineering, and firmware analysis work.

---

## Project Goals

This lab was built to support the following goals:

- implement a secure bootloader on the STM32F411CEU6 Black Pill
- sign the application image on the host
- verify the application using **SHA-256** and **ECDSA-P256**
- fail closed on invalid headers, invalid vectors, or invalid signatures
- build a diagnostic application for testing, firmware comparison, and later reverse engineering workflows

This project also serves as a foundation for later work in:

- fail-closed validation testing
- clean vs modified firmware comparison
- hidden-trigger analysis
- Ghidra-based reverse engineering
- flash protection and hardening experiments
- controlled extraction testing on protected devices

---

## Hardware Used

- **MCU board:** STM32F411CEU6 Black Pill
- **Debugger / flasher:** ST-Link V2
- **UART / interface work:** Tigard or FTDI adapter
- **Console:** USART1
- **Status LED:** PC13

---

## Flash Layout

This is the working memory map used by the project.

- **Bootloader:** `0x08000000 .. 0x08007FFF` (32 KB)
- **Image header:** `0x08008000 .. 0x080081FF` (512 bytes)
- **Application vectors + code:** `0x08008200` and above

This split is intentional:

- the bootloader occupies the first 32 KB of flash
- the signed image header is placed immediately after the bootloader
- the application vector table starts at `0x08008200`

---

## What the Bootloader Does

The bootloader is responsible for:

- validating the image header
- validating the application vector table
- calculating SHA-256 over the application image
- verifying an ECDSA-P256 signature using an embedded public key
- jumping to the application only if verification succeeds
- failing closed if validation fails

This gives the project a simple but real authenticated boot chain on Cortex-M.

---

## Project Structure

```text
stm32-secure-boot-lab/
├── bootloader/
├── app/
├── tools/
├── docs/
├── images/
└── README.md
```

Suggested usage of each folder:

- `bootloader/`
  STM32CubeIDE bootloader project and related crypto integration
- `app/`
  application project, clean build, and later modified / hidden-trigger variants
- `tools/`
  helper scripts such as image packing and public key conversion
- `docs/`
  project notes, flash maps, validation notes, and future hardening documentation
- `images/`
  hardware photos, diagrams, screenshots, and lab visuals

## Bootloader Project

Project name: `Project_1_1_Bootloader`

### Important Bootloader Files

- `mbedtls_config_boot.h`
- `image_header.h`
- `verify.c`
- `main.c`
- `pack_image.py`
- `public_key_to_c.py`

### Embedded Crypto Notes

The bootloader uses the ST-packaged Mbed TLS copy with a minimum working source set that includes:

- `bignum.c`
- `bignum_core.c`
- `constant_time.c`
- `ecp.c`
- `ecp_curves.c`
- `ecdsa.c`
- `sha256.c`
- `platform_util.c`

Important implementation note:

The ST package behaved like a split Mbed TLS 3.x-style tree. In addition to `bignum.c`, the build also required:

- `bignum_core.c`
- `constant_time.c`

These were necessary to resolve missing `mbedtls_ct_*` and `mbedtls_mpi_core_*` symbols.

### Bootloader Size

Final successful build was approximately:

- `text = 22136`
- `data = 92`
- `bss = 1980`

This keeps the bootloader comfortably inside the 32 KB flash region.

## Application Project

Project name: `Project_1_2_App`

The application is a visible diagnostic payload used to confirm that authenticated boot succeeds and that control transfers cleanly from the bootloader.

### Application Features

- USART1 diagnostic console
- LED heartbeat on PC13
- interactive commands:
  - `STATUS`
  - `VERSION`
  - `LEDON`
  - `LEDOFF`

### Application Configuration

- USART1: `PA9 / PA10`
- LED: `PC13`
- Serial: `115200 8N1`
- Linked for flash execution
- VTOR relocation enabled

### Linker / Placement Notes

A key fix in the application project was correcting linker behavior so the image actually lived in flash instead of behaving like a RAM-debug layout.

Correct placement:

- `.isr_vector -> FLASH`
- `.text -> FLASH`
- `.rodata -> FLASH`
- `.data -> RAM with AT > FLASH`
- `.bss -> RAM`

### VTOR Relocation

In `system_stm32f4xx.c`:

```c
#define USER_VECT_TAB_ADDRESS
#define VECT_TAB_OFFSET  0x00008200U
```

This ensures the application uses the correct vector table after the bootloader jump.

## Critical Validation Fix

One of the most important bugs found during development was in the application vector sanity check.

### Original Problem

The bootloader reported:

```text
[boot] invalid vectors
[boot] fail closed
```

This happened even though the header and application vector region were correct.

Observed valid vectors:

- `SP = 0x20020000`
- `Reset = 0x08008AF5`

### Incorrect Stack Pointer Check

```c
if ((sp & 0x2FFE0000UL) != 0x20000000UL) return 0;
```

That logic incorrectly rejected `0x20020000`, which is valid on STM32F411.

### Fixed Validation Logic

```c
if (sp < 0x20000000UL || sp > 0x20020000UL) return 0;
if ((sp & 0x3U) != 0U) return 0;
if ((rv & 1U) == 0U) return 0;
if ((rv & ~1U) < APP_VECTOR_ADDR || (rv & ~1U) >= FLASH_END_ADDR) return 0;
```

After this fix, the bootloader correctly accepted the application image and transferred control.

## Signing and Packing Flow

The host-side signing workflow uses:

- `private.pem`
- `public.der`

The public key is converted into a C array using `public_key_to_c.py` and embedded in the bootloader verification logic.

### Pack Command

```bash
python3 pack_image.py Project_1_2_App.bin app_packed.bin --key private.pem --version 1
```

This produces a packed image with:

- a signed 512-byte image header at `0x08008000`
- application vectors and code starting at `0x08008200`

## Flashing Flow

Flash order:

1. flash `Bootloader.elf` to `0x08000000`
2. flash `app_packed.bin` to `0x08008000`

This preserves the intended bootloader/header/application layout.

## Known-Good UART Output

The following output confirms the expected working path:

```text
[boot] secure boot starting
[boot] signature valid

=================================
[app] Application Boot Successful
[app] Diagnostic Console Enabled
Commands: STATUS, VERSION, LEDON, LEDOFF
=================================
[app] tick...
```

This confirms that:

- the bootloader starts correctly
- the image header is accepted
- signature verification succeeds
- application vectors are valid
- the jump to the application succeeds
- UART output from the application is working

## Wiring Notes

### SWD

- `SWDIO -> DIO`
- `SWCLK -> SCK`
- `GND -> GND`
- `3.3V` only if intentionally powering from ST-Link

### UART

- `PA9 / USART1_TX -> adapter RX`
- `PA10 / USART1_RX -> adapter TX`
- `GND -> GND`

### Linux Serial Notes

On Kali, the Tigard / FTDI setup appeared as:

- `/dev/ttyUSB0`
- `/dev/ttyUSB1`

The correct console output was on:

- `/dev/ttyUSB0`

Working commands:

```bash
picocom -b 115200 /dev/ttyUSB0
picocom -b 115200 --echo /dev/ttyUSB0
```

## Hidden Trigger and Reverse Engineering Variant

A later extension of the lab adds an intentional hidden trigger for reverse engineering and binary diffing work.

In the modified application:

- the main loop continues to produce normal visible behavior
- `USART1_IRQHandler` monitors inbound UART data
- if it receives the exact six-byte sequence `REBOOT`
- it sets a volatile flag
- the main loop then calls `NVIC_SystemReset()`

This keeps the modified build useful as a controlled firmware analysis target.

## Important Cross-Stage CPU State Lesson

The bootloader disables interrupts before jumping to the application.

Because the application inherits that CPU state, the interrupt-driven trigger did not work until interrupts were explicitly re-enabled at the start of the application:

```c
__enable_irq();
```

That detail is important for both debugging and later reverse engineering writeups.

## Reverse Engineering Notes

This project is also intended as a raw firmware analysis target.

### Ghidra Import Notes

When importing the application binary as a raw Cortex-M image, use:

- Base address: `0x08008200`

### Good Hunt Points

Useful anchors for analysis include:

- the `REBOOT` string in the modified build
- references to the compare logic
- the volatile trigger flag
- the eventual reset path

A clean application and a modified hidden-trigger application make a good pair for binary diffing and Ghidra comparison.

## Phase 3 Direction: Protection and Hardening

The next hardening direction for this project is:

- apply `RDP Level 1`
- apply targeted `WRP`
- attempt controlled firmware extraction from a protected device
- document what remains accessible and what is blocked
- preserve known-good binaries and keys outside the board
- treat rollback from protected state as a mass-erase event

Important practical rule:

- `RDP Level 1` is the reusable research setting
- `RDP Level 2` is not intended for a board you want to keep reusing

This keeps the project aligned with realistic device hardening while preserving the board for later labs.

## Blog / Article Series

This project is already documented as part of the STM32F411 Secure Boot Lab series on Graytips.

Published pages:

- Series page:  
  `https://graytips.com/iot/stm32f411-secure-boot-lab-series/`
- Part 1:  
  `https://graytips.com/iot/stm32f411-secure-boot-lab/`
- Part 2:  
  `https://graytips.com/iot/stm32f411-secure-boot-part-2-validation-hidden-triggers-re/`

## Repository Context

This lab is part of the broader `IoT_Projects` repository, where I collect IoT, embedded security, and firmware research projects.

Main repository:

- `https://github.com/gr4ytips/IoT_Projects`

## Author

Shafeeque Olassery Kunnikkal  
aka `gr4ytips`

- GitHub: `https://github.com/gr4ytips`
- Blog: `https://graytips.com`

## Disclaimer

This project is for educational, research, and defensive security purposes. Any techniques, tooling, or workflows documented here should be used only on hardware, firmware, and systems you own or are explicitly authorized to test.

---

## License

Unless otherwise noted, this project follows the repository license:

Apache License 2.0
