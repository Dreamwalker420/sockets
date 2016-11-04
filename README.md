Author: KirkLeroy J. Powell

Description: Sockets Labs for CS407

Lab #1
* Establish Client-Server Socket Communication
* <rembash> protocol exchange
* Fork server to execute Bash
* Fork client to handle command line input from client
* Client displays Bash output
* Handle multiple clients

Lab #2
* Convert code to using functions
* Fork server to handle client
* Server pseudoterminal
* Fork server sub-process to execute Bash on pseudo-terminal
* Server relays PTY data from terminal to master
* Client fork to handle command line input to server socket
* Client  relay data from terminal to display

Lab #3
* Use a thread on the server to invoke epoll API for handling input and output to and from Bash to client
* Use a thread on the server to handle a new client, then fork a process to exec Bash
* Prevent malicious client attack with timers
* Use a signal to kill server thread on timeout
* Close client socket on timeout

Lab #4
* Implement a thread pool for workers to handle io tasks
* Use a library to access thread pool
* Implement a job queue (FIFO)
* Ensure mutex properly functions to limit access to job queue
* Create a test program to analyze use of thread pool
* Submit files in tarball