# smallsh - A Small Shell Program

This project implements a custom shell called `smallsh`, written in C. It supports basic Unix-like shell functionality including command execution, I/O redirection, background processes, built-in commands, and signal handling for foreground-only mode.

---

## 📦 Features

- Custom prompt (`:`) for entering commands.
- Supports built-in commands:
  - `cd [path]` – change directory (defaults to HOME if no path is provided)
  - `status` – print the exit status or terminating signal of the last foreground process
  - `exit` – exit the shell and kill any remaining background processes
- Input/output redirection using `<` and `>`
- Background process support using `&` at end of command
- Foreground-only mode toggle using `CTRL+Z`
- `$$` variable expansion for shell's PID
- Graceful handling of background process cleanup

---

## 🧠 How It Works

- **Input Parsing**: Accepts up to 2048-character lines from standard input.
- **Command Expansion**: Replaces `$$` with the shell's current PID.
- **Execution**: Commands are executed via `fork()` and `execvp()`.
- **Redirection**: Input/output is redirected using `dup2()` when specified.
- **Signal Handling**:
  - `SIGINT` (`CTRL+C`) is ignored by the parent but affects foreground children.
  - `SIGTSTP` (`CTRL+Z`) toggles foreground-only mode.
- **Background Processes**: Runs jobs in the background and checks for their completion with `waitpid(..., WNOHANG)`.

---

## 🛠️ Build Instructions

To build the program, use the `gcc` compiler:

```bash
gcc -o smallsh main.c -std=c99
```

Make sure you have a `smallsh.h` header file defining the following structures:

```c
// smallsh.h

#ifndef SMALLSH_H
#define SMALLSH_H

struct Command {
    char* command;
    char** args;
    int numArgs;
    char* input_file;
    char* output_file;
    int foreground;
    char* string_copy;
};

struct Shell {
    int* bg_pids;
    int num_bg_pids;
    int bg_capacity;
    int exit_status;
};

#endif
```

---

## 🚀 Run Instructions

Once compiled:

```bash
./smallsh
```

You will see a `:` prompt to enter commands.

---

## 💡 Example Usage

```bash
: ls -l
: echo hello > file.txt
: sort < file.txt &
: status
: cd /tmp
: sleep 5 &
: exit
```

Use `CTRL+Z` to toggle foreground-only mode (background `&` will be ignored).

---

## 🧹 Cleanup

When exiting, all background processes are terminated. Output includes notices like:

```
background pid 12345 is done: exit value 0
```

---

## 🧑‍💻 Author

This shell was developed as part of a systems programming assignment to simulate behavior similar to a minimalistic version of bash.

---

## ⚠️ Notes

- Maximum argument count is 512 (plus the command itself).
- Commands longer than 2048 characters will be truncated.
- Some edge cases and advanced shell features are not supported (e.g., piping, history, environment variable export).

---

Happy shelling! 🐚
