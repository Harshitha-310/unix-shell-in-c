# Custom Unix Shell in C

Features:

- Single command execution
- Sequential execution (##)
- Parallel execution (&&)
- Pipelines (|)
- Output redirection (>)
- Append redirection (>>)
- Command history (history, !!, !n)
- Built-in cd command
- Signal handling (Ctrl+C, Ctrl+Z ignored)

Concepts used:

fork(), execvp(), waitpid()
pipe(), dup2()
process management
file descriptor manipulation
signal handling
