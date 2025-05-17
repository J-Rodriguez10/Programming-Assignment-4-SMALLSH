<div align="center">
  <img width="200" alt="Terminal Shell Icon" src="https://upload.wikimedia.org/wikipedia/commons/4/4b/Bash_Logo_Colored.svg" />
</div>

## 🖥️ About the Project

**smallsh** is a custom Unix-style command line shell implemented in C as a portfolio assignment for Oregon State University's ***CS 344: Operating Systems I***. It replicates core features of popular shells like Bash, including process management, I/O redirection, background execution, and signal handling.

This project demonstrates a deep understanding of Linux system calls, process control, and signal APIs through a clean and functional CLI experience.

> 💡 Created to explore Linux internals by Jesus Rodriguez

---

## ⚙️ Built With

### 🧠 Core Technologies
- **C (GNU99 standard)**
- **POSIX system calls**
- `fork()`, `execvp()`, `waitpid()`
- `dup2()` for I/O redirection
- `signal.h` for `SIGINT` and `SIGTSTP` handlers

---

## ✨ Features

- 📜 **Command parsing** (up to 2048 chars / 512 arguments)
- 🚫 **Ignores comments and blank lines**
- 🧠 **Built-in commands**: `exit`, `cd`, `status`
- 🌐 **Runs all other commands** using child processes
- 🔁 **Input/output redirection** via `<` and `>` operators
- 🎭 **Foreground & background execution** with `&`
- 🚦 **Signal handling**:
  - `Ctrl+C` (`SIGINT`): terminates foreground-only children
  - `Ctrl+Z` (`SIGTSTP`): toggles foreground-only mode
- 🧼 **Tracks background processes** and reports their termination

---

## 🧪 Example Usage
<img width="1088" alt="Image" src="https://github.com/user-attachments/assets/64ebfee5-768a-4fa1-b1a3-ed650fd506cc" />
<img width="1083" alt="Image" src="https://github.com/user-attachments/assets/f832dc50-8ddb-49dd-91b1-2b3b8513e554" />
