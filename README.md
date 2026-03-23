# IoT_Projects

A public repository for my IoT, embedded security, and firmware research projects.

This repo is where I collect hands-on labs, proof-of-concept builds, notes, and supporting material from my work on microcontrollers, firmware integrity, reverse engineering, hardware interfaces, and related embedded security topics.

The goal is not just to publish finished code, but to document the engineering process, debugging journey, and research workflow behind each project.

---

## Current Focus

The current flagship project in this repository is:

### STM32F411 Secure Boot Lab

A compact cryptographic secure boot lab built on the **STM32F411CEU6 Black Pill**, designed to validate firmware before execution using:

- **SHA-256**
- **ECDSA-P256**
- structured image headers
- vector table validation
- fail-closed boot logic

This lab is being developed as both:

- a practical firmware integrity exercise
- a reusable embedded security research platform

It is also intended to support later work in:

- tamper testing
- binary diffing
- reverse engineering in Ghidra
- UART trigger analysis
- write/read protection hardening
- broader IoT security research

---

## What This Repository Covers

Projects in this repo may include work in areas such as:

- secure boot and firmware integrity
- STM32 and ARM Cortex-M research
- UART, SWD, JTAG, and hardware debug workflows
- firmware packaging and signing
- reverse engineering and binary analysis
- embedded exploitation labs
- hardware-backed defensive controls
- IoT device security experimentation

---

## Repository Structure

The repository is organized by project.

A typical layout looks like this:

```text
IoT_Projects/
├── stm32-secure-boot-lab/
│   ├── bootloader/
│   ├── app/
│   ├── tools/
│   ├── docs/
│   ├── images/
│   └── README.md
├── future-project-folder/
│   └── ...
└── README.md
```

Each project should ideally contain:

- source code
- build or flash instructions
- helper scripts
- project-specific documentation
- screenshots, diagrams, or lab media where useful

---

## Featured Project: STM32 Secure Boot Lab

The STM32 secure boot lab is centered around a bootloader/application split with a fixed flash layout.

### Memory Map

- **Bootloader:** `0x08000000 .. 0x08007FFF` (32 KB)
- **Image header:** `0x08008000 .. 0x080081FF` (512 bytes)
- **Application vectors + code:** `0x08008200` and above

### Bootloader Responsibilities

- validate image header
- validate application vectors
- calculate SHA-256 over the application image
- verify ECDSA-P256 signature using an embedded public key
- jump to the application only if valid
- fail closed if validation fails

### Platform

- **MCU:** STM32F411CEU6 Black Pill
- **Debugger:** ST-Link V2
- **Console / interface work:** Tigard / FTDI UART setup
- **IDE:** STM32CubeIDE
- **Crypto integration:** ST-packaged Mbed TLS

### Why This Lab Matters

This project is more than a secure boot demo.

It is being built as a foundation for later embedded security workflows, including:

- signed vs modified firmware comparison
- RE-oriented payload analysis
- Ghidra-based firmware inspection
- protection and hardening experiments
- defensive research into boot-chain assumptions and failure cases

---

## Project Philosophy

I prefer projects that are:

- practical
- reproducible
- technically meaningful
- useful as future research platforms

That means this repository may contain not only working builds, but also debugging notes, implementation decisions, and lessons learned from getting things to work correctly on real hardware.

---

## Blog / Writeups

Project writeups and longer-form documentation are published on **Graytips**.

These articles are used to document both the final implementation and the real engineering path behind the work, including design decisions, mistakes, fixes, and next-step research ideas.

---

## Work in Progress

This repository is actively evolving.

Some projects may be:

- incomplete
- mid-refactor
- documentation-first
- code-first with cleanup still pending

That is expected. The repo reflects active research and lab development, not just polished end-state releases.

---

## Author

**Shafeeque Olassery Kunnikkal**  
aka **gr4ytips**

GitHub: [gr4ytips](https://github.com/gr4ytips)

---

## Disclaimer

This repository is for educational, research, and defensive security purposes.

Any techniques, tooling, or workflows documented here should be used responsibly and only on systems, hardware, or firmware you own or are explicitly authorized to test.

---

## License

Unless otherwise noted, the code and project materials in this repository are licensed under the **Apache License 2.0**.

This choice keeps the repository open and reusable for research, learning, and engineering work, while also providing clear terms around attribution, redistribution, modification, and patent rights.

See the [LICENSE](./LICENSE) file for the full license text.