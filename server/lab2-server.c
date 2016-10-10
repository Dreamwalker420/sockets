/* Kirk Powell
 * CS 407 - Lab 2 (Server)
 * October 4, 2016
 * 
 * Many of the structures of this program were obtained from outside sources.  They are cited on the line where they are used.  For broad interpretation of overall structure see the following sources:
 *
 * Sources:
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

// Feature Test Macros
// For pseudalterminal device (PTY)
#define _GNU_SOURCE // might not compile on Solaris, FreeBSD, Mac OS X, etc. (http://stackoverflow.com/questions/5378778/what-does-d-xopen-source-do-mean)
#include <stdlib.h>
#include <fcntl.h>
int posix_openpt(int flags);
int grantpt(int mfd);
int unloackpt(int mfd);
char *ptsname(int mfd);

#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
// Need this to read input from sockets into a string per requirements
#include "readline.c"

// Set shared secret
#define SECRET "<cs407rembash>\n"
// Set the port to be used by server and clients
#define PORT 4070
// Set buffer size for pty slave name
#define MAX_SNAME 1000

// Declare functions
int create_client_socket(int server_sockfd);
int create_pty(char *pty_slave_name);
int create_server_socket();
int handle_client(int connect_fd);
int handle_pty(int connect_fd, const struct termios *ttyOrig);
int protocol_exchange(int connect_fd);

// Global variable for PTY
struct termios ttyOrig;

// Reset terminal mode on program exit
static void ttyReset(void){
	if(tcsetattr(STDIN_FILENO, TCSANOW, &ttyOrig) == -1){
		// TODO: Fix this to the correct error handler
		// errExit("tcsetattr");
	}
}

// Begin main()
int main(){
	// TODO: Verify how this works
	#ifdef DEBUG
		printf("Server is running ...");
	#endif

	// Server and client socket file descriptors
	int server_sockfd, client_sockfd;

	// Call create_server_socket()
	server_sockfd = create_server_socket();

	// Ignore when a child process closes
	signal(SIGCHLD, SIG_IGN);

	// Begin listening on the socket
	while(1) {
		// Continue to accept new clients	
		// printf("server waiting ...\n");
		
		// Call create_client_socket()
		// TODO: Error check this?
		client_sockfd = create_client_socket(server_sockfd);

		// Acknowledge new client
		// printf("processing new client ...\n");

		// Start a sub-process to handle the client
		switch(fork()){
			case -1:
				// Error
				perror("Server: Unable to start a sub-process to handle client.");
				exit(EXIT_FAILURE);
			case 0:
				// Child process to handle client
				// Call handle_client() here
				// TODO: Since this is a fork here, I need to error check the function and close it
				if(handle_client(client_sockfd) == -1){
					perror("Server: Unable to handle client.");
					// Terminate server if it cannot handle clients
					exit(EXIT_FAILURE);
				}
		}
		// A successful fork should create a subprocess to handle the client and return the server to listening for another connection.
		// This is an infinite loop
		// CTRL-C in the terminal will stop the server from running
	}
}
// End of Main



// Functions

// Called by main server loop to create a new client socket
// Returns the client socket file descriptor
int create_client_socket(int server_sockfd){
	int client_sockfd, client_len;
	struct sockaddr_in client_address;
	client_len = sizeof(client_address);
	// Important!! --> Server blocks on accept() call.
	if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len)) == -1){
		fprintf(stderr, "Server: Unable to create sockets for clients.");
		// Notify server and continue listening for new clients
		return -1;
	}
	// Success return client socket
	return client_sockfd;
}
// End of create_client_socket()


// Called by handle_pty to create pty device for client
// Stores the pty slave name and returns the master file descriptor
// Must store pty_slave_name for use by other functions
int create_pty(char *pty_slave_name){
	// Open PTY master device
	int master_fd, savedErrno;
	if((master_fd = posix_openpt(O_RDWR | O_NOCTTY)) == -1){
		return -1;
	}

	// Grant permissions for PTY slave
	if(grantpt(master_fd) == -1){
		savedErrno = errno;
		close(master_fd);
		errno = savedErrno;
		return -1;
	}

	// Unlock PTY slave
	if(unlockpt(master_fd) == -1){
		savedErrno = errno;
		close(master_fd);
		errno = savedErrno;
		return -1;
	}

	// Get PTY slave name
	char *p;
	p = ptsname(master_fd);
	if(p == NULL){
		savedErrno = errno;
		close(master_fd);
		errno = savedErrno;
		return -1;	
	}

	if(strlen(p) < MAX_SNAME){
		strncpy(pty_slave_name,p,MAX_SNAME);
	}
	else{
		close(master_fd);
		errno = EOVERFLOW;
		return -1;	
	}
	return master_fd;
}
// End of create_pty()


// Called by main server loop to create a server socket
int create_server_socket(){
	int server_sockfd, server_len;
	struct sockaddr_in server_address;

	// Create server socket
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Server: Unable to create server socket.");
		// Terminate server
		exit(EXIT_FAILURE);
	}

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(PORT);
	server_len = sizeof(server_address);
	// Bind socket to server
	int check_bind;
	if((check_bind = bind(server_sockfd, (struct sockaddr *)&server_address, server_len)) == -1){
		perror("Server: Unable to bind socket to server.");
		// Terminate server
		exit(EXIT_FAILURE);
	}

	// Assign server to listen on the socket
	int listening;
	if((listening = listen(server_sockfd, 5)) == -1){
		perror("Server: Unable to listen on server socket.");
		// Terminate server
		exit(EXIT_FAILURE);
	}

	// Allow kernel to begin using the port again when server terminated
	int i=1;
	int check_option;
	if((check_option = setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i))) == -1){
		perror("Server: Unable to inform kernel to reuse server socket.");
		// Terminate server
		exit(EXIT_FAILURE);
	}

	return server_sockfd;
}
// End of create_server_socket


// Called by main server loop to handle client connections
// The socket file descriptor assigned to the client is connect_fd
int handle_client(int connect_fd){
	// Call protocol_exchange()
	if(protocol_exchange(connect_fd) == -1){
		perror("Server: Unable to complete protocol exchange with client.");
		close(connect_fd);
		// Terminate server sub-process for handling client connections
		exit(EXIT_FAILURE);
	}

	// Confirm new client
	// printf("new client confirmed.\n");

	// Open a PTY
	pid_t cpid;

	// Retrieve the attributes of the terminal
	if(tcgetattr(STDIN_FILENO, &ttyOrig) == -1){
		perror("Server: Unable to retrieve attributes of the terminal.");
		close(connect_fd);
		// Terminate server sub-process for handling client connections
		exit(EXIT_FAILURE);
	}

	// Create a pseudoterminal for client
	// Call to handle_pty()
	cpid = handle_pty(connect_fd, &ttyOrig);
	switch(cpid){
		case -1:
			// This occurs when the server cannot fork to create a place for Bash to execute
			// Terminate this process
			perror("Server: Unable to handle client PTY.");
			close(connect_fd);
			// Terminate server sub-process for handling client connections
			exit(EXIT_FAILURE);
		case 0:
			// Success
			// Return to server to handle new client
			close(connect_fd);
			exit(EXIT_SUCCESS);
	}

	// I should never get here ...
	perror("Server: Something wrong with handle_pty() function.");
	close(connect_fd);
	// Terminate server sub-process for handling client connections
	exit(EXIT_FAILURE);
}
// End of handle_client()


// Called by handle_client()
int handle_pty(int connect_fd, const struct termios *ttyOrig){
	// This is the server sub-process
	int master_fd, slave_fd, savedErrno;
	pid_t bash_pid;

	// Need a loction for the pty_slave_name
	char pty_slave_name[MAX_SNAME];
	
	// Create pseudoterminal for bash to execute on
	master_fd = create_pty(pty_slave_name);
	if(master_fd == -1){
		perror("Server: Unable to create PTY. Cancelled client connection.");
		// End client cycle instead of server
		close(connect_fd);
		// Terminate server sub-process handling client connection
		exit(EXIT_FAILURE);
	}

	// Create a tertiary sub-process to handle Bash on a pseudoterminal
	bash_pid = fork();
	switch(bash_pid){
		case -1:
			// Check if fork failed, still in sub-process for server to handle client
			perror("Server: Unable to create sub-process for Bash to execute in.");
			close(master_fd);
			close(connect_fd);
			// If I can't fork, terminate the server sub-process
			exit(EXIT_FAILURE);
		case 0:
			// This is creating a tertiary sub-process with inheritance of file descriptors

			// This is not needed in the tertiary server sub-process
			close(master_fd);

			// Child is the leader of the new session and looses its controlling terminal.
			if(setsid() == -1){
				perror("Server: Unable to set a new session.");
				close(connect_fd);
				// Terminate tertiary server sub-process if I am unable to set a new session
				exit(EXIT_FAILURE);
			}

			// Set PTY slave as the controlling terminal
			slave_fd = open(*pty_slave_name, O_RDWR);

			if(slave_fd == -1){
				perror("Server: Unable to set controlling terminal.");
				close(slave_fd);
				close(connect_fd);
				// Terminate tertiary server sub-process
				exit(EXIT_FAILURE);
			}

			if(&ttyOrig != NULL){
				if(tcsetattr(slave_fd, TCSANOW, ttyOrig) == -1){
					perror("Server: Unable to set PTY slave attributes.");
					close(slave_fd);
					close(connect_fd);
					// Terminate tertiary server sub-process
					exit(EXIT_FAILURE);				
				}
			}

			// Child process needs redirection
			// Set up redirection
			// stdin, stdout, stderr must be redirected to client connection socket

			// TODO: I think the STDIN_FILENO needs to be replaced with the connect_fd here???
			if(dup2(slave_fd, STDIN_FILENO) != STDIN_FILENO){
				perror("Server: Unable to redirect standard input.");
				close(connect_fd);
				// If I can't fork, terminate the server
				exit(EXIT_FAILURE);
			}
			if(dup2(slave_fd, STDOUT_FILENO) != STDOUT_FILENO){
				perror("Server: Unable to redirect standard output.");
				close(connect_fd);
				// If I can't fork, terminate the server
				exit(EXIT_FAILURE);
			}
			if(dup2(slave_fd, STDERR_FILENO) != STDERR_FILENO){
				perror("Server: Unable to redirect standard error output.");
				close(connect_fd);
				// If I can't fork, terminate the server
				exit(EXIT_FAILURE);
			}

			// TODO: Determine what this is for
			if(slave_fd > STDERR_FILENO){
				close(slave_fd);
			}

			// Start Bash
			execlp("bash", "bash", NULL);

			// Handle error code from Bash failure
			perror("Server: Unable to execute Bash in terminal.");
			// If Bash failed, terminate tertiary server sub-process, file descriptors get erased on exit
			exit(EXIT_FAILURE);			
	}

	// Bash executing in new sub-process with pseudoterminal
	// Server sub-process still needs to do a few things
	
	// TODO: Determine what this is for
	// ttySetRaw(STDIN_FILENO, &ttyOrig);

	// Resets terminal when server sub-process terminates
	if(atexit(ttyReset) != 0){
		perror("Server: Unable to reset terminal.");
		// Terminate server sub-process handling client connection
		exit(EXIT_FAILURE);
	}

	// Return to server sub-process to allow it to close successfully
	return 0;
}
// End of handle_pty()

// Called by handle_client() for protocol exchange with server
int protocol_exchange(int connect_fd){
	int nwrite;
	// Send server protocol to client
	char *server_protocol = "<rembash>\n";		
	if((nwrite = write(connect_fd, server_protocol, strlen(server_protocol))) == -1){
		perror("Server: Unable to communicate protocol to client.");
		return -1;
	}

	// Verify client shared secret
	char *from_client = readline(connect_fd);
	if(strcmp(from_client, SECRET) != 0){
		char *write_error = "<error>\n";
		if((nwrite = write(connect_fd, write_error, strlen(write_error))) == -1){
			perror("Server: Error notifying client of incorrect shared secret.");
			// End client cycle instead of server
			close(connect_fd);
			return -1;
		}
		// Acknowledge client rejected
		// printf("Client Token Rejected.");
		return -1;
	}

	// Confirm shared secret
	char *confirm_protocol = "<ok>\n";
	if((nwrite = write(connect_fd, confirm_protocol, strlen(confirm_protocol))) == -1){
		perror("Server: Error notifying client of confirmed shared secret.");
		return -1;
	} 
}
// End of protocol exhcange