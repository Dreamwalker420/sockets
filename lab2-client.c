/* Kirk Powell
 * CS 407 - Lab 2 (Client)
 * October 4, 2016
 * 
 * Many of the structures of this program were obtained from outside sources.  They are cited on the line where they are used.  For broad interpretation of overall structure see the following sources:
 *
 * Sources:
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

#define DEBUG 1

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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
// Need this to read input from sockets into a string per requirements
#include "server/readline.c"

// Set shared secret
#define SECRET "<cs407rembash>\n"
// Set the port to be used
#define PORT 4070
// Set buffer size for pty slave name
#define MAX_SNAME 1000
// Set buffer size for communications between Bash and PTY
#define BUFFER_SIZE 4096

// Declare functions
int configure_client_socket(char *IP_ADDRESS);
int connect_client_to_server(int sockfd);
int create_pty(char *pty_slave_name);
int tty_set_raw(int fd);

// Global variable for PTY
struct termios ttyOrig;

// Reset terminal mode on program exit
static void ttyReset(void){
	if(tcsetattr(STDIN_FILENO, TCSANOW, &ttyOrig) == -1){
		perror("Server: Unable to resert terminal.");
		exit(EXIT_FAILURE);
	}
}


// Begin main()
int main(int argc, char *argv[]){
	#ifdef DEBUG
		printf("Initializing client ...\n");
	#endif

	char *IP_ADDRESS;
	// Capture command line argument & check for valid ipaddress
	if(argc == 2){
		// Capture IP ADDRESS
		IP_ADDRESS = argv[1];

		// Acknowledge command line input
		#ifdef DEBUG
			printf("Processing command: ./client %s\n", argv[1]);
		#endif
	}
	else{
		// Handle incorrect command line entry
		fprintf(stderr, "Usage: ./client [IP_ADDRESS]\n");
		fprintf(stderr, "Example: ./client 127.0.0.1\n");
		// Terminate client connection
		exit(EXIT_FAILURE);
	}

	// Configure client connection
	int sockfd;
	sockfd = configure_client_socket(IP_ADDRESS);
	if(sockfd == -1){
		fprintf(stderr, "Client: Cannot intialize a socket.");
		// Terminate client connection
		exit(EXIT_FAILURE);
	}

	// Connect to server (inlcudes protocol exchange)
	if(connect_client_to_server(sockfd) == -1){
		fprintf(stderr, "Client: Cannot connect to server socket.");
		close(sockfd);
		// Terminate client connection
		exit(EXIT_FAILURE);		
	}

	// Retrieve the attributes of the terminal
	if(tcgetattr(STDIN_FILENO, &ttyOrig) == -1){
		perror("Client: Unable to retrieve attributes of the terminal.");
		close(sockfd);
		// Terminate client connection
		exit(EXIT_FAILURE);
	}

	// Create a pseudoterminal
	int master_fd, slave_fd, savedErrno;
	// Need a loction for the pty_slave_name
	char pty_slave_name[MAX_SNAME];
	
	// Create pseudoterminal
	master_fd = create_pty(pty_slave_name);
	if(master_fd == -1){
		perror("Client: Unable to create PTY. Cancelled client connection.");
		close(sockfd);
		// Terminate client connection
		exit(EXIT_FAILURE);
	}

	int nread, nwrite;
	char from_socket[BUFFER_SIZE];
	char command[512];
	// Start a new subprocess to listen to terminal commands and send to PTY
	pid_t read_cpid;
	switch(read_cpid = fork()){
		case -1:
			// Unable to fork, close client
			perror("Client: Error trying to create sub-process to handle terminal commands.");
			close(sockfd);
			// Terminate client sub-process
			exit(EXIT_FAILURE);
		case 0:
			// This is not needed in the client sub-process
			close(master_fd);

			// Child is the leader of the new session and looses its controlling terminal.
			if(setsid() == -1){
				perror("Client: Unable to set a new session.");
				close(sockfd);
				// Terminate client sub-process if I am unable to set a new session
				exit(EXIT_FAILURE);
			}

			// Set PTY slave as the controlling terminal
			slave_fd = open(pty_slave_name, O_RDWR);
			if(slave_fd == -1){
				perror("Client: Unable to set controlling terminal.");
				close(slave_fd);
				close(sockfd);
				// Terminate client sub-process
				exit(EXIT_FAILURE);
			}

			if(&ttyOrig != NULL){
				if(tcsetattr(slave_fd, TCSANOW, &ttyOrig) == -1){
					perror("Client: Unable to set PTY slave attributes.");
					close(slave_fd);
					close(sockfd);
					// Terminate client sub-process
					exit(EXIT_FAILURE);				
				}
			}
			// Child process to read command lines from terminal
			// Read from client socket
			while((nread = read(sockfd,command,BUFFER_SIZE)) > 0){
				// Write to PTY master
				if((nwrite = write(master_fd,command, nread)) == -1){
					perror("Server: Unable to write to PTY master.");
					// Terminate client sub-process
					exit(EXIT_FAILURE);
				}
			}
			if(errno){
				perror("Client: Error when reading input command.");
				// Terminate client sub-process
				exit(EXIT_FAILURE);
			}
			// Terminate client sub-process
			exit(EXIT_SUCCESS);
	}
	
	// Client sub-process reading terminal inputs

	// Resume client process

	// Set noncanonical mode
	if(tty_set_raw(STDIN_FILENO) == -1){
		fprintf(stderr, "Client: Unable to switch to noncanonical mode.");
		// Terminate client connection
		exit(EXIT_FAILURE);
	}


	// Reset terminal when server sub-process terminates
	if(atexit(ttyReset) != 0){
		perror("Client: Unable to reset terminal.");
		// Terminate client connection
		exit(EXIT_FAILURE);
	}

	// Start a secondary sub-process to read from socket to stdout
	pid_t terminal_cpid;
	switch(terminal_cpid = fork()){
		case -1:
			perror("Client: Error trying to create a sub-process to handle terminal output");
			// Terminate client secondry sub-process
			exit(EXIT_FAILURE);
		case 0:
			// Read input from client to relay to server
			while((nread = read(sockfd, from_socket, BUFFER_SIZE)) > 0){
				// Write to socket
				if((nwrite = write(master_fd,from_socket,nread)) == -1){
					// Handle error writing to buffer
					perror("Client: Unable to write command to Bash");
					// Terminate client secondary sub-process
					exit(EXIT_FAILURE);
				}
			}
			if(errno){
				// Parent error reading from buffer
				perror("Client: Unable to read from Bash");
				// Terminate client secondary sub-process
				exit(EXIT_FAILURE);
			}			
	}
	
	// Client secondary sub-process doing something now also

	// Resume client process

	// Termindate child processes
	wait(NULL);
	kill(read_cpid,SIGTERM);
	kill(terminal_cpid,SIGTERM);

	// Acknowledge client is done
	#ifdef DEBUG
		printf("Closing client socket.\n");
	#endif

	close(sockfd);
	exit(EXIT_SUCCESS);
}
// End of Main


// Functions

// Called by main() to intialize a client socket
// Returns socket file descriptor on success
int configure_client_socket(char *IP_ADDRESS){
	int sockfd;
	struct sockaddr_in address;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Client unable to create a socket.");
		return -1;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);

	// Validate IP address
	int check_IP;
	// Use command line argument for IP_ADDRESS here
	if((check_IP = inet_aton(IP_ADDRESS, &address.sin_addr)) == 0){
		fprintf(stderr, "Invalid IP Address.\n");
		close(sockfd);
		return -1;
	}
	else if (check_IP == -1){
		fprintf(stderr, "Unable to convert IP Address.");
		close(sockfd);
		return -1;
	}
	else {
		// IP ADDRESS is valid
	}
	return sockfd;
}
// End of configure_client_socket()


// Called by main() to connect client socket to server)
// Returns 0 on success
int connect_client_to_server(int sockfd){
	// Connect to server
	// TODO: Fix this compile error on the &address
	if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1){
		perror("Client unable to connect to server");
		close(sockfd);
		return -1;
	}

	// Check server protocol
	char *protocol = "<rembash>\n";
	char *server_protocol = readline(sockfd);
	if(strcmp(protocol,server_protocol) != 0){
		fprintf(stderr, "Incorrect Protocol.\n");	
		close(sockfd);
		return -1;
	}

	// Acknowledge protocol
	#ifdef DEBUG
		printf("Protocol confirmed.\n");
	#endif

	// Send shared secret
	int nwrite;
	if((nwrite = write(sockfd, SECRET, strlen(SECRET))) == -1){
		perror("Unable to send secret to client.");
		close(sockfd);
		return -1;
	}
	
	// Acknowledge secret has been sent
	#ifdef DEBUG
		printf("Sent server secret code.\n");
	#endif

	// Check confirmation from server
	char *confirm_protocol = readline(sockfd);
	if(strcmp(confirm_protocol,"<ok>\n") != 0){
		fprintf(stderr, "Server Unable to Confirm Handshake.\n");
		close(sockfd);
		return -1;
	}

	// Acknowledge client-server connection
	#ifdef DEBUG
		printf("Connection to server established.\n");
	#endif

	return 0;
}
// End of connect_client_to_server()


// Called by main() to create PTY device
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


// Called by main() to set noncanonical mode
// Returns 0 on successful confirgurations
int tty_set_raw(int fd){
	struct termios t;

	// Get terminal attributes
	if(tcgetattr(fd, &t) == -1){
		return -1;
	}

	// Check if attributes have already been set
	if(&ttyOrig != NULL){
		ttyOrig = t;
	}

	// From the book: "Noncanonical mode, disables signals, extended input processing, and echoing"
	t.c_lflag &= ~(ICANON | IEXTEN | ECHO);

	// From the book: "Disable special handling of CR, NL, and BREAK.  No 8th-bit stripping or parity error handling. Disable START/STOP output flow control."
	t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);

	// From the book: "Disable all output processing"
	t.c_oflag &= ~OPOST;

	// Only one character at a time
	t.c_cc[VMIN] = 1;

	// BLOCK!
	t.c_cc[VTIME] = 0;

	// Set new attributes
	if(tcsetattr(fd, TCSAFLUSH, &t) == -1){
		return -1;
	}

	return 0;
}
// End of tty_set_raw()