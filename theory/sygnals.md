Why does onSignal need an int parameter?
The signal handler function (like onSignal) must match the signature expected by the system: void handler(int signum).
When a signal (like SIGINT) is delivered, the kernel calls the handler and passes the signal number (e.g., SIGINT = 2) as an int argument.
Even if you don't use the value, you must declare the parameter so the function matches the expected type.
=======================================

sigaction is a system call to set up how the program handles signals (like SIGINT, SIGTERM).
It lets you specify a handler function, and other options, in a struct sigaction.

What does sigemptyset do?
Signals can be blocked while the handler runs (to avoid reentrancy issues).
sigemptyset(&sa.sa_mask); initializes the mask to “no signals blocked” during the handler.
If you wanted to block other signals while handling one, you’d use sigaddset to add them.
========================================

SIGINT and SIGTERM are signals used in Unix/Linux systems to control processes:

SIGINT (Signal Interrupt): Sent when you press Ctrl+C in the terminal. It tells the process to interrupt (stop) what it’s doing. By default, it terminates the process, but you can handle it for graceful shutdown.

SIGTERM (Signal Terminate): Sent by default when you run kill <pid>. It politely asks the process to terminate. The process can catch this signal to clean up before exiting.

Both are used to stop programs, but SIGTERM is a general termination request, while SIGINT is specifically for user interrupts (like Ctrl+C).


A process is an instance of a running program. It has its own memory, system resources, and execution context (like variables, open files, and program counter). Each process is managed by the operating system and identified by a unique process ID (PID). Multiple processes can run the same program independently.
===========================================