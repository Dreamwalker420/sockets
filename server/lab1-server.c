/* Kirk Powell
 * CS 407 - Labb 1 (Server)
 * September 19, 2016
 * 
 * A note about the code here:  The bulk of this structure was taken from "Beginning Linux Programming" [pgs 604-677, 4th edition] by Matthew and Stone.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(PORT);
	server_len = sizeof(server_address);
	bind(server_sockfd, (struct sockaddr *)&server_address, server_len);

	listen(server_sockfd, 5);

	// TODO: Does this sufficiently collect child processes?  I don't think so
	signal(SIGCHLD, SIG_IGN);

	// Continue to accept new clients	
	printf("server waiting\n");

	while(1) {
		client_len = sizeof(client_address);
		client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);

		// Call handle_client here to invoke function
		handle_client(client_sockfd);
		// TODO: How to exit here in a loop to look for new connections!
	}
	exit(0);
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
		errno = 1;
		perror("Client Token Rejected");
		close(connect_fd);
		exit(1);
	}
	// Confirm shared secret
	char *confirm_protocol = "<ok>\n";
	write(connect_fd, confirm_protocol, strlen(confirm_protocol)); // TODO: Can I error check this?

	// Set up redirection
	// stdin, stdout, stderr must be redirected to client connection socket
	// dup2() system call to change FDs

	// Spawn process to handle client
	if(fork() == 0) {
		// Start Bash
		// use 'execlp("bash","bash","--noediting","-i",NULL)'
		printf("I'm a real process!\n");
		// Must close when bash is terminated

		int test = 0;
		int converted;		
		while(test < 10){
			sleep(5);
			converted = htonl(test);				
			printf("server: %d\n", test);			
			write(connect_fd, &converted, sizeof(converted));
			test++;
			
		}
		close(connect_fd);

		printf("client process completed.");
		exit(0);
	}
	else {
		close(connect_fd);
		exit(1);
	}
}
// End of handle_client()
