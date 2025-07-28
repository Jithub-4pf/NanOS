# NanOS TODO

- Make IPC blocking: block processes on receive if queue is empty, wake on message arrival
- Add more complex message types: support structs, message types/IDs
- Implement a simple shell: use IPC to communicate between shell and other processes
- Add system calls: let user processes use IPC and other kernel services via syscalls
- Experiment with more processes: launch more tasks that communicate via messages
- Add user mode support: privilege separation, user/kernel boundary
- Add process statistics: track CPU usage, sleep time, etc.
- Implement per-process address spaces: virtual memory, isolation
- Add process blocking on I/O: allow processes to block waiting for keyboard or other events
- Add stack tracing and better diagnostics
- Improve kernel panic handler 