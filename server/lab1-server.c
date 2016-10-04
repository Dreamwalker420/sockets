/* Kirk Powell
 * CS 407 - Labb 1 (Server)
 * September 19, 2016
 * 
 * A note about the code here:  The bulk of this structure was taken from "Beginning Linux Programming" [pgs 604-677, 4th edition] by Matthew and Stone.
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

// Set shared secret for testing
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

	// TODO: Check this system call
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(PORT);
	server_len = sizeof(server_address);
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len);

	listen(server_sockfd, 5);

	// TODO: Does this sufficiently collect child processes?  I don't think so
	signal(SIGCHLD, SIG_IGN);

	while(1) {
		// Continue to accept new clients	
		printf("server waiting ...\n");
		// Configure client socket		
		client_len = sizeof(client_address);
		client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);
		// Acknowledge new client
		printf("processing new client ...\n");
		// Call handle_client here to invoke function
		handle_client(client_sockfd);
	}
}
// End of Main




// Called by main server loop to handle client connections
void handle_client(int connect_fd){
	// Send server protocol to client
	char *server_protocol = "<rembash>\n";		
	write(connect_fd, server_protocol, strlen(server_protocol));
	// Verify client shared secret
	char *from_client = readline(connect_fd);
	if(strcmp(from_client, SECRET) != 0){
		char *write_error = "<error>\n";
		write(connect_fd, write_error, strlen(write_error));
		printf("Client Token Rejected.");
		close(connect_fd);
		exit(EXIT_FAILURE);
	}
	// Confirm shared secret
	char *confirm_protocol = "<ok>\n";
	write(connect_fd, confirm_protocol, strlen(confirm_protocol)); // TODO: Can I error check this?

	// Confirm new client
	printf("new client confirmed.\n");

	// Set up redirection
	// stdin, stdout, stderr must be redirected to client connection socket
	dup2(connect_fd, 0);
	// dup2(connect_fd, 1);
	// dup2(connect_fd, 2);
		
	// Spawn process to handle client
	if(fork() == 0) {


		// Start Bash
		execlp("bash","bash","--noediting","-i",NULL);
		
		close(connect_fd);
		exit(EXIT_SUCCESS);
	}
	else {
		close(connect_fd);
		printf("unable to handle client process.\n");
	}
	return;
}
// End of handle_client()
