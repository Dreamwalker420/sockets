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
* Use a thread to invoke epoll API for handling read
* Use a thread on the server to handle a new client
* Prevent malicious client attack with timers