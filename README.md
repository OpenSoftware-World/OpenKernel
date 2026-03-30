# 🚀 PatiOS-OpenKernelBased

> A modern, modular, and learning-oriented operating system kernel project.

---

## 📌 About The Project

**OpenKernel** is a kernel development project designed to improve low-level system programming skills and to understand modern operating system architecture.

This project aims to:

* Interact directly with hardware
* Develop kernel-level services
* Build a minimal and understandable architecture

---

## 🎯 Goals

* 🧠 Simple and learnable kernel architecture
* ⚙️ Development of core kernel services
* 🖥️ Low-level hardware control (CPU, memory, I/O)
* 📦 Modular structure (expandable and maintainable)
* 🔒 Secure and controlled system behavior

---

## 🛠️ Technologies Used

* **Language:** C / x86 Assembly
* **Compiler:** GCC (i686-elf)
* **Assembler:** NASM
* **Emulator:** QEMU / VMware

---

## 📂 Project Structure

```bash
OpenKernel/
│── Boot/          # Bootloader and initialization code
│── Kernel/        # Kernel core
│── Drivers/       # Hardware drivers
│── SystemLib/           # Helper functions (string, memory, etc.)
```

---

## ⚙️ Features (Current / Planned)

* [x] VGA text mode output system
* [x] Basic CPU control (CLI, HLT, etc.)
* [x] Kernel service infrastructure
* [ ] Memory management
* [ ] Interrupt handling (IDT)
* [x] Keyboard driver
* [ ] File system (basic FS)

---

## 🚀 Build & Run

### Requirements

* `gcc (cross-compiler)`
* `nasm`
* `qemu`

### Build

```bash
make
```

### Run

```bash
make run
```

---

## ⚠️ Disclaimer

This project is intended for educational and development purposes.

The user assumes all responsibility for any potential issues or damages caused by using this software.

---

## 🤝 Contributing

Contributions are always welcome!

1. Fork the repository
2. Create a new branch
3. Make your changes
4. Submit a Pull Request

---

## 📜 License

This project is open source.
For more details, see the `LICENSE` file.

---

## 👨‍💻 Developer

**OpenSoftware-World**

---

## 🌍 Vision

OpenKernel is not just a project,

it is a starting point for those who want to learn low-level system development.
