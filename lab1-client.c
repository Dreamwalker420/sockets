/* Kirk Powell
 * CS 407 - Lab 1 (Client)
 * September 19, 2016
 * 
 * A note about the code here:  The structure was taken from "Beginning Linux Programming" [pgs 604-677, 4th edition] by Matthew and Stone.
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
// Need this to read input from sockets into a string per requirements
#include "server/readline.c"

#define BUFFER_SIZE 4096
// Set shared secret
#define SECRET "<cs407rembash>\n"
// Set the port to be used
#define PORT 4070

// Declare functions
bool isValidIpAddress(char *ipAddress);

int main(int argc, char *argv[]){
	int nwrite;

	char *IP_ADDRESS;
	// Capture command line argument & check for valid ipaddress
	if((argc == 2) && (isValidIpAddress(argv[1]) != 0)){	
		// Capture IP ADDRESS
		IP_ADDRESS = argv[1];

		// Acknowledge command line input
		// printf("Processing command: ./client %s\n", argv[1]);
	}
	else{
		// Handle incorrect command line entry
		printf("Usage: ./client [IP_ADDRESS]\n");
		printf("Example: ./client 127.0.0.1\n");
		exit(EXIT_FAILURE);
	}

	int sockfd;
	struct sockaddr_in address;

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
		perror("Client unable to create a socket.");
		// Terminate client
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	// Use command line argument for IP_ADDRESS here
	inet_aton(IP_ADDRESS, &address.sin_addr);
	address.sin_port = htons(PORT);

	// Connect to server
	int check_connection;
	if((check_connection = connect(sockfd, (struct sockaddr *)&address, sizeof(address))) == -1){
		perror("Client1 unable to connect to server");
		close(sockfd);
		// Terminate client
		exit(EXIT_FAILURE);
	}

	// Check server protocol
	char *protocol = "<rembash>\n";
	char *server_protocol = readline(sockfd);
	if(strcmp(protocol,server_protocol) != 0){
		// printf("Incorrect Protocol.\n");	
		close(sockfd);
		// Terminate client
		exit(EXIT_FAILURE);
	}

	// Acknowledge protocol
	// printf("Protocol confirmed.\n");

	// Send shared secret
	if((nwrite = write(sockfd, SECRET, strlen(SECRET))) == -1){
		perror("Unable to send secret to client.");
		close(sockfd);
		// Terminate client
		exit(EXIT_FAILURE);
	}
	
	// Acknowledge secret has been sent
	// printf("Sent server secret code.\n");

	// Check confirmation from server
	char *confirm_protocol = readline(sockfd);
	if(strcmp(confirm_protocol,"<ok>\n") != 0){
		printf("Server Unable to Confirm Handshake.\n");
		close(sockfd);
		// Terminate client
		exit(EXIT_FAILURE);
	}

	// Acknowledge client-server connection
	// printf("Connection to server established.\n");

	// Start a new subprocess to listen to terminal commands
	pid_t cpid;
	if((cpid = fork()) == 0){
		// Read command lines from terminal
		// Use fgets, assume < 512 characters
		char command[512];
		while(fgets(command, sizeof(command), stdin) != NULL){
			// Write to server socket
			write(sockfd, command, strlen(command));
		}
		if(errno){
			perror("Client error when reading input command.");
			// Terminate client
			exit(EXIT_FAILURE);
		}
		// Terminate client
		exit(EXIT_SUCCESS);
	}
	else if (cpid == -1){
		// Close client, error forking
		close(sockfd);
		// Terminate client
		exit(EXIT_FAILURE);
	}
	else {
		// Success
		// Resume Parent Process
		// Read from socket to stdout
		int nread;
		char from_socket[BUFFER_SIZE];		
		// Read input from client to relay to server
		while((nread = read(sockfd, from_socket, BUFFER_SIZE)) > 0){
			// Write to socket
			if((nwrite = write(1,from_socket,nread)) == -1){
				// Handle error writing to buffer
				perror("Unable to write command to Bash");
				// Terminate client
				exit(EXIT_FAILURE);
			}
		}
		if(errno){
			// Parent error reading from buffer
			perror("Client unable to read from Bash");
			// Terminate client
			exit(EXIT_FAILURE);
		}
		close(sockfd);
		// Terminate client and return command line control
		exit(EXIT_SUCCESS);	
	}

	// Clean-up child processes
	// Termindate child process
	wait(NULL);
	kill(cpid,SIGTERM);

	// Acknowledge client is done
	// printf("Closing client socket.\n");
	close(sockfd);
	exit(EXIT_SUCCESS);
}
// End of Main

// http://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ip-address-in-c
bool isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}
// End of isValidIpAddress