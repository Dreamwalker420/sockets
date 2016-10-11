/* Kirk Powell
 * CS 407 - Lab 3 (Server)
 * October 9, 2016
 * 
 * Sources:
 	CS407 Lab Solutions by NJ Carver
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

// TODO: Turn this off when submitting for grading
#define DEBUG 1

// Feature Test Macro for pseudalterminal device (PTY)
#define _XOPEN_SOURCE 600

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Declared Constants
// Set shared secret
#define SECRET "<cs407rembash>\n"
// Set the port to be used by server and clients
#define PORT 4070
// Set buffer size for pty slave name
#define MAX_SNAME 1000
// Set buffer size for communications between Bash and PTY
#define BUFFER_SIZE 4096

// Declare functions
int create_pty(char *pty_slave_name);
int create_server_socket();
void handle_client(int connect_fd);
int protocol_exchange(int connect_fd);
void pty_socket_relay(int connect_fd, int master_fd);
void run_pty_shell(char *pty_slave_name);
void sigchld_handler(int signal, siginfo_t *sip, void *ignore);

// Global variable of child processes to track signals.  Only need to track two.
int cpid[2];

// Begin main()
int main(){
	#ifdef DEBUG
		printf("Starting server ...\n");
	#endif

	// Server and client socket file descriptors
	int server_sockfd, client_sockfd;

	// Call create_server_socket()
	if((server_sockfd = create_server_socket()) == -1){
		exit(EXIT_FAILURE);
	}

	// Ignore when a child process closes
	signal(SIGCHLD, SIG_IGN);

	// Begin listening on the socket
	while(1){
		// Continue to accept new clients
		#ifdef DEBUG
			printf("Server waiting ...\n");
		#endif

		// Important!! --> Server blocks on accept() call.
		if((client_sockfd = accept(server_sockfd, (struct sockaddr *) NULL, NULL)) != -1){
			// Notify server and continue listening for new clients
			#ifdef DEBUG
				printf("Processing new client ...\n");
			#endif

			// A successful client should fork to a sub-process to handle the client and return the server to listening for another connection.
			// Start a sub-process to handle the client ONLY if  new client was accepted
			switch(fork()){
				case -1:
					// Error creating sub-process for handling a client
					perror("Server: Unable to start a sub-process to handle client.");
					// Terminate server
					exit(EXIT_FAILURE);
				case 0:
					// Child process to handle client
					#ifdef DEBUG
						printf("Processing new client ...\n");
					#endif

					// Don't leave open file descriptors
					close(server_sockfd);
					// Call handle_client() here, this does not return a value, all error checking is handled inside the function
					handle_client(client_sockfd);
					// Should never get here ...
					exit(EXIT_FAILURE);
				default:
					// Very important to close this file descriptor in case there is an error in the fork call
					close(client_sockfd);
			}
		}
		// Any attempt to accept a new client should return back to the server regardless of success
	}
	// This is an infinite loop
	// CTRL-C in the terminal will stop the server from running

	// Should never get here ...
	exit(EXIT_FAILURE);
}
// End of Main



// Functions


// Called by handle_client() to create pty device for client
// Stores the pty slave name and returns the master file descriptor on success
// Must store pty_slave_name for use by other functions
int create_pty(char *pty_slave_name){
	// Open PTY master device
	int master_fd;
	if((master_fd = posix_openpt(O_RDWR)) == -1){
		return -1;
	}

	// Unlock PTY slave
	// Error check not needed here because it can only fail if file descriptor is an error
	unlockpt(master_fd);

	// Get PTY slave name
	char *p;
	p = ptsname(master_fd);
	if(p == NULL){
		return -1;	
	}

	if(strlen(p) < MAX_SNAME){
		strncpy(pty_slave_name,p,MAX_SNAME);
	}
	else{
		errno = EOVERFLOW;
		return -1;	
	}
	return master_fd;
}
// End of create_pty()


// Called by main server loop to create a server socket
// Returns a server socket file descriptor on success
int create_server_socket(){
	int server_sockfd;
	struct sockaddr_in server_address;

	// Create server socket
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Server: Unable to create server socket.");
		return -1;
	}

	// Carver uses this in lab #2 solution
	// Set up server adress struct, zero-out anything that might be there
	memset(&server_address,0,sizeof(server_address));

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(PORT);

	// Allow kernel to begin using the port again when server terminated
	int i=1;
	if(setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == -1){
		perror("Server: Unable to inform kernel to reuse server socket.");
		return -1;
	}

	// Bind socket to server
	if(bind(server_sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1){
		perror("Server: Unable to bind socket to server.");
		return -1;
	}

	// Assign server to listen on the socket
	if(listen(server_sockfd, 10) == -1){
		perror("Server: Unable to listen on server socket.");
		return -1;
	}

	return server_sockfd;
}
// End of create_server_socket


// Called by main server loop to handle client connections
// The socket file descriptor assigned to the client is connect_fd
void handle_client(int connect_fd){
	// Call protocol_exchange()
	if(protocol_exchange(connect_fd) == -1){
		fprintf(stderr, "Server: Unable to complete protocol exchange with client.");
		// Terminate server sub-process for handling client connections
		exit(EXIT_FAILURE);
	}

	// Confirm new client and create a pseudoterminal
	#ifdef DEBUG
		printf("Client-Server Protocol Exchange Completed.\n");
		printf("Create PTY for client with file descriptor %d.\n", connect_fd);
	#endif

	// This is the server sub-process
	int master_fd;

	// Need a loction for the pty_slave_name
	char pty_slave_name[MAX_SNAME];
	
	// Create pseudoterminal for bash to execute on
	if((master_fd = create_pty(pty_slave_name)) == -1){
		perror("Server: Unable to create PTY. Cancelled client connection.");
		// Terminate server sub-process handling client connection
		exit(EXIT_FAILURE);
	}

	#ifdef DEBUG
		printf("PTY Created.  Start sub-process for Bash to be executed in.\n");
	#endif

	// Create a secondary sub-process to handle Bash on a pseudoterminal
	switch(cpid[0] = fork()){
		case -1:
			// Check if fork failed, still in sub-process for server to handle client
			perror("Server: Unable to create sub-process for Bash to execute in.");
			// If I can't fork, terminate the server sub-process
			exit(EXIT_FAILURE);
		case 0:
			// This is creating a secondary sub-process with inheritance of file descriptors

			#ifdef DEBUG
				printf("Server sub-process for Bash.\n");
			#endif

			// The PTY Master file descriptor is not needed in the secondary server sub-process
			close(master_fd);

			run_pty_shell(pty_slave_name);

			// If Bash failed, terminate tertiary server sub-process, file descriptors get erased on exit
			exit(EXIT_FAILURE);			
	}

	// Bash executing in new sub-process with pseudoterminal
	// Return of control flow to server sub-process
	
	pty_socket_relay(connect_fd,master_fd);

	// Collect any remaining child processes
	while (waitpid(-1,NULL,WNOHANG) > 0);

	// Terminate server sub-process for handling client connection
	exit(EXIT_SUCCESS);
}
// End of handle_client()

// Called by handle_client() for protocol exchange with server
// Returns 0 on successful protocol exchange
int protocol_exchange(int connect_fd){
	int nread, nwrite;
	// Send server protocol to client
	char *server_protocol = "<rembash>\n";		
	if((nwrite = write(connect_fd, server_protocol, strlen(server_protocol))) == -1){
		perror("Server: Unable to communicate protocol to client.");
		// End client cycle instead of server
		return -1;
	}

	// Verify client shared secret
	char from_client[513];
	if((nread = read(connect_fd,from_client,512)) == -1){
		perror("Server: Error reading from client.");
		return -1;
	}
	from_client[nread] = '\0';
	if(strcmp(from_client, SECRET) != 0){
		char *write_error = "<error>\n";
		if((nwrite = write(connect_fd, write_error, strlen(write_error))) == -1){
			perror("Server: Error notifying client of incorrect shared secret.");
			// End client cycle instead of server
			return -1;
		}

		// Acknowledge client rejected
		#ifdef DEBUG
			printf("Client Token Rejected.\n");
		#endif

		// End client cycle instead of server
		return -1;
	}

	// Confirm shared secret
	char *confirm_protocol = "<ok>\n";
	if((nwrite = write(connect_fd, confirm_protocol, strlen(confirm_protocol))) == -1){
		perror("Server: Error notifying client of confirmed shared secret.");
		// End client cycle instead of server
		return -1;
	} 

	// Protocol exchange completed
	return 0;
}
// End of protocol exhcange


// Called by handle_client() to relay data between PTY and socket
void pty_socket_relay(int connect_fd, int master_fd){
	#ifdef DEBUG
		printf("Set-up server PTY to socket relay.\n");
	#endif

	// I didn't implement a signal handler on my lab#2, this is taken explicity from the Lab #2 solution
	struct sigaction act;

	//Setup SIGCHLD handler to deal with child process terminating unexpectedly:
	//(Must be done before fork() in case child immediately terminates.)
	act.sa_sigaction = sigchld_handler;  //Note use of .sa_sigaction instead of .sa_handler
	act.sa_flags = SA_SIGINFO|SA_RESETHAND;  //SA_SIGINFO required to use .sa_sigaction instead of .sa_handler
	sigemptyset(&act.sa_mask);
	if (sigaction(SIGCHLD,&act,NULL) == -1) {
		perror("Server: Error registering handler for SIGCHLD");
		exit(EXIT_FAILURE); 
	}

	// Relay data between BASH and PTY
	int nread, nwrite, total;
	char from_client[BUFFER_SIZE];
	char from_bash[BUFFER_SIZE];
	// Create a tertiary server sub-process to read from client and write to PTY master
	switch(cpid[1] = fork()){
		case -1:
			// Check if fork failed, still in sub-process for server to handle client
			perror("Server: Unable to create sub-process for reading from client.");
			// If I can't fork, terminate the server sub-process
			exit(EXIT_FAILURE);
		case 0:
			// This creates a tertiary server sub-process for handling reading from client and writing to PTY master
			#ifdef DEBUG
				printf("New server sub-process to read from socket and write to PTY (to Bash).\n");
			#endif

			// Read from client socket and writing to PTY (out to Bash)
			nwrite = 0;
			while(nwrite != -1 && (nread = read(connect_fd,from_client,BUFFER_SIZE)) > 0){
				total = 0;
				do{
					// Write to PTY master
					if((nwrite = write(master_fd,from_client+total, nread-total)) == -1){
						break;
					}
					// Keep reading from the buffer until it is done
					total += nwrite;
				}while(total < nread);
			}
			if(errno){
				// Server tertiary sub-process error reading from buffer
				perror("Server: Unable to read from client output");
			}
			else{
				fprintf(stderr, "Server: Connection to client closed.\n");
			}
			// Terminate server tertiary sub-process
			// Should not get here
			exit(EXIT_FAILURE);
	}

	// Server tertiary sub-process handling output from client and writing to PTY master input 
	// Return of control flow to server sub-process
	// Read from PTY slave and write to client

	// Read from bash
	nwrite = 0;
	while(nwrite != -1 && (nread = read(master_fd,from_bash,BUFFER_SIZE)) > 0){
		total = 0;
		do{
			// Write to client
			if((nwrite = write(connect_fd,from_bash+total,nread-total)) == -1){
				break;
			}
			total += nwrite;
			// Keep reading from the buffer until it is done
		}while(total < nread);
	}
	if(errno){
		// Server secondary sub-process error reading from buffer
		perror("Server: Unable to read from Bash output.");
	}

	// No more data to transfer
	// Need to remove SIGCHLD handler and set to ignore so it wont collect
	act.sa_handler = SIG_IGN;
	if(sigaction(SIGCHLD,&act,NULL) == -1){
		perror("Server: Error ignoring SIGCHLD.");
	}

	// Terminate child processes
	kill(cpid[0],SIGTERM);
	kill(cpid[1],SIGTERM);

	// action completed
	return;
}
// End of  pty_socket_relay()


// Called by handle_client() to run PTY shell
void run_pty_shell(char *pty_slave_name){
	// Child is the leader of the new session and looses its controlling terminal.
	if(setsid() == -1){
		perror("Server: Unable to set a new session.");
		// Terminate secondary server sub-process if I am unable to set a new session
		exit(EXIT_FAILURE);
	}

	// Set PTY slave as the controlling terminal
	int slave_fd;
	if((slave_fd = open(pty_slave_name, O_RDWR)) == -1){
		perror("Server: Unable to set controlling terminal.");
		// Terminate secondary server sub-process
		exit(EXIT_FAILURE);
	}

	// Child process needs redirection
	// stdin, stdout, stderr must be redirected to client connection socket
	if(dup2(slave_fd, STDIN_FILENO) != STDIN_FILENO){
		perror("Server: Unable to redirect standard input.");
		// If I can't fork, terminate the server
		exit(EXIT_FAILURE);
	}
	if(dup2(slave_fd, STDOUT_FILENO) != STDOUT_FILENO){
		perror("Server: Unable to redirect standard output.");
		// If I can't fork, terminate the server
		exit(EXIT_FAILURE);
	}
	if(dup2(slave_fd, STDERR_FILENO) != STDERR_FILENO){
		perror("Server: Unable to redirect standard error output.");
		// If I can't fork, terminate the server
		exit(EXIT_FAILURE);
	}

	// This will print infinitely, don't leave in!
	// #ifdef DEBUG
	// 	printf("Server sub-process ready to execute Bash.\n");
	// #endif

	// Start Bash
	execlp("bash", "bash", NULL);

	// Handle error code from Bash failure
	perror("Server: Unable to execute Bash in terminal.");
	// Terminate secondary server sub-process
	exit(EXIT_FAILURE);
}
// End of run_pty_shell()


// Handler for SIGCHLD during relay_data_between_socket_and_pty().
// Required to deal with premature/unexpected termination of child
// process that is handling data transfer PTY<--socket.
// Can get invoked by PTY<--socket child or bash child.
// Makes sure both children of client handler process are terminated,
// then terminates the PTY-->socket (handle_client) process too.
// (No point in continuing, as would just immediately get read/write
// error and then terminate anyway.)
void sigchld_handler(int signal, siginfo_t *sip, void *ignore)
{
  #ifdef DEBUG
  //printf("SIGCHLD handler!\n");
  printf("SIGCHLD handler invoked due to PID: %d\n",sip->si_pid);
  #endif

  //Terminate other child process:
  if (sip->si_pid == cpid[0])
    kill(cpid[1],SIGTERM);
  else
    kill(cpid[0],SIGTERM);

  //Collect any/all killed child processes for this client:
  while (waitpid(-1,NULL,WNOHANG) > 0);

  //Terminate the parent without returning since no point:
  exit(EXIT_SUCCESS);
}