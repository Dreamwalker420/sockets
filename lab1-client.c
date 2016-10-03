/* Kirk Powell
 * CS 407 - Lab 1 (Client)
 * September 19, 2016
 * 
 * A note about the code here:  The bulk of this structure was taken from "Beginning Linux Programming" [pgs 604-677, 4th edition] by Matthew and Stone.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "server/readline.c"

#define BUFFER_SIZE 4096
#define SECRET "<cs407rembash>\n"
#define PORT 4070

int main(int argc, char *argv[]){
	char *IP_ADDRESS;
	// Capture command line argument
	if(argc == 2){	
		// TODO: Validate the IP Address
		printf("Example: ./client %s\n", argv[1]);
		IP_ADDRESS = argv[1];
	}
	else{
		printf("Usage: ./client [IP_ADDRESS]\n");
		printf("Example: ./client 127.0.0.1\n");
		exit(1);
	}

	int sockfd;
	int len;
	struct sockaddr_in address;
	int result;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	address.sin_family = AF_INET;
	// Use command line argument for IP_ADDRESS here
	//address.sin_addr.s_addr = inet_addr(IP_ADDRESS);
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(PORT);
	len = sizeof(address);

	result = connect(sockfd, (struct sockaddr *)&address, len);

	if(result == -1){
		perror("Client1 unable to connect to server.\n");
		exit(EXIT_FAILURE);
	}

	// Check server protocol
	char *protocol = "<rembash>\n";	
	char *server_protocol = readline(sockfd);
	if(strcmp(protocol,server_protocol) != 0){
		printf("Incorrect Protocol.\n");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	// Acknowledge protocol
	printf("Protocol confirmed.\n");
	// Send shared secret
	write(sockfd, SECRET, strlen(SECRET));
	printf("Sent server secret code.\n");
	// Check confirmation & handle error
	char *confirm_protocol = readline(sockfd);
	if(strcmp(confirm_protocol,"<ok>\n") != 0){
		printf("Server Unable to Confirm Handshake.\n");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	printf("Connection to server established.\n");
/*
	// Start a new subprocess
	if(fork() == 0){
		// Read command lines from terminal
		// Use fgets assume < 512 characters
		
		// Terminate when 'file-end return' or error

		// Write to server socket

		// Terminate on error

		printf("Client awaits your command!");
	}
	else{
		errno = 1;
		perror("Unable to start subprocess to handle client requests");
		close(sockfd);
		exit(1);
	}
*/
	int test,nread;
	while((nread = read(sockfd, &test, sizeof(test))) > 0){
		test = ntohl(test);
		printf("from server: %d\n", test);
	}

	// // Handle input from subprocess, send to server
	// while(1){
	// 	// Read output from socket

	// 	// Terminate on error
	
	// 	// Write to users terminal
		
	// 	// Terminate on error
	// 	printf("I'm listening ...\n");
	// 	sleep(1);
	// }

	printf("Closing client socket.\n");
	close(sockfd);
	exit(EXIT_SUCCESS);
}
