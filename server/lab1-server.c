/* Kirk Powell
 * CS 407 - Labb 1 (Server)
 * September 19, 2016
 * 
 * The bulk of this structure was taken from "Beginning Linux Programming" [pgs 604-677, 4th edition] by Matthew and Stone.
 *
 * Thomas Ruble was instrumental in helping me understand how to resolve several final issues with my code.
 *
 * I've commented out the system messages.  Like comments in the code, system messages seem like intuitively good programming.  However, as Thomas mentions, they technically exceed the specifications of the lab.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h> // TODO: Double check that I need this for dup2()
#include <unistd.h>
// Need this to read input from sockets into a string per requirements
#include "readline.c"

// Set shared secret
#define SECRET "<cs407rembash>\n"
// Set the port to be used by server and clients
#define PORT 4070

// Declare functions
void handle_client(int connect_fd);

int main(){
	int server_sockfd, client_sockfd;
	int server_len, client_len;
	struct sockaddr_in server_address;
	struct sockaddr_in client_address;

	// Create server socket
	if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Unable to create server socket.");
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
		perror("Unable to bind socket to server.");
		// Terminate server
		exit(EXIT_FAILURE);
	}

	// Assign server to listen on the socket
	int listening;
	if((listening = listen(server_sockfd, 5)) == -1){
		perror("Unable to listen on server socket.");
		// Terminate server
		exit(EXIT_FAILURE);
	}

	// Ignore when a child process closes
	signal(SIGCHLD, SIG_IGN);

	// Begin listening on the socket
	while(1) {
		// Continue to accept new clients	
		// printf("server waiting ...\n");
		
		// Configure client socket		
		client_len = sizeof(client_address);
		if((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len)) == -1){
			perror("Unable to create socket for client.");
			// Terminate server
			exit(EXIT_FAILURE);
		}
		// Acknowledge new client
		// printf("processing new client ...\n");

		// Call handle_client here to invoke function
		handle_client(client_sockfd);
	}
}
// End of Main




// Called by main server loop to handle client connections
void handle_client(int connect_fd){
	int nwrite;
	// Send server protocol to client
	char *server_protocol = "<rembash>\n";		
	if((nwrite = write(connect_fd, server_protocol, strlen(server_protocol))) == -1){
		perror("Unable to communicate protocol to client.");
		// End client cycle instead of server
		close(connect_fd);
		return;
	}

	// Verify client shared secret
	char *from_client = readline(connect_fd);
	if(strcmp(from_client, SECRET) != 0){
		char *write_error = "<error>\n";
		if((nwrite = write(connect_fd, write_error, strlen(write_error))) == -1){
			perror("Error notifying client of incorrect shared secret.");
			// End client cycle instead of server
			close(connect_fd);
			return;
		}
		// Acknowledge client rejected
		// printf("Client Token Rejected.");
		
		// End client cycle instead of server
		close(connect_fd);
		return;
	}

	// Confirm shared secret
	char *confirm_protocol = "<ok>\n";
	if((nwrite = write(connect_fd, confirm_protocol, strlen(confirm_protocol))) == -1){
		perror("Error notifying client of confirmed shared secret.");
		// End client cycle instead of server
		close(connect_fd);
		return;
	} 

	// Confirm new client
	// printf("new client confirmed.\n");

	// Spawn process to handle client
	pid_t cpid;
	cpid = fork();
	if (cpid == 0) {
		// Child process needs redirection
		// Set up redirection
		// stdin, stdout, stderr must be redirected to client connection socket
		dup2(connect_fd, 0);
		dup2(connect_fd, 1);
		dup2(connect_fd, 2);

		// I can't accurately explain this as Thomas did.  Essentially, Bash will get confused on who owns or controls the terminal and will create conflicts with multiple clients.
		setsid();

		// Start Bash
		// This is creating a "new" process with inheritance of file descriptors
		// execlp returns -1 on error
		execlp("bash","bash","--noediting","-i",NULL);

		// Handle error code from Bash failure
		perror("Unable to execute Bash in terminal.");
		// If Bash failed, terminate client, file descriptors get erased on exit
		exit(EXIT_FAILURE);
	}
	else if (cpid == -1) {
		// Check if fork failed
		perror("Unable to create subprocess to handle client.\n");
		close(connect_fd);
		// If I can't fork, terminate the server
		exit(EXIT_FAILURE);
	}
	else {
		// Success!
		close(connect_fd);
	}
	return;
}
// End of handle_client()