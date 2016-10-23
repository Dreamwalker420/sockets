/* Kirk Powell
 * CS 407 - Lab 3 (Server)
 * October 9, 2016
 * 
 * Compile Using this format:
 * $ gcc -Wall lab3-server.c -o server.exe -pthread -lrt
 *
 * Works with lab3-client.c
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
// TODO: Determine if this is necessary since I've included _GNU_SOURCE
#define _XOPEN_SOURCE 600
// Feature Test Macro for timers
#define _POSIX_C_SOURCE 199309L
// Feature Test Macro for accept4() call
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
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
// Set the maximum number of events for epoll to handle
#define MAX_EVENTS 5
 // Set the clock to be used by timer
#define CLOCKID CLOCK_REALTIME

// Declare functions
int create_pty(char *pty_slave_name);
int create_server_socket();
void *handle_client(void *arg);
void *handle_epoll();
int protocol_exchange(int connect_fd);
void run_pty_shell(char *pty_slave_name, int connect_fd);
void signal_handler(int sig, siginfo_t *si, void *uc);

// Global variables
// Child process ids to track for signals.  Only need to track two.
int cpid[2];
// File descriptor for epoll
int epoll_fd;
// Pairs of file descriptors for epoll to scan.  All initialized to 0
int epoll_fd_pairs[100] = { 0 };
struct epoll_event ev;
struct epoll_event evlist[MAX_EVENTS];

// Begin main()
int main(){
	#ifdef DEBUG
		printf("Starting server ...\n");
	#endif

	// Server and client socket file descriptors
	int server_sockfd, client_sockfd;

	// Call create_server_socket()
	if((server_sockfd = create_server_socket()) == -1){
		// Error instantiating the socket for the server
		perror("Server: Unable to instantiate the server socket.");
		// Terminate server process
		exit(EXIT_FAILURE);
	}

	// Ignore when a child process closes
	signal(SIGCHLD, SIG_IGN);

	#ifdef DEBUG
		printf("Server preparing ePoll API ...\n");
	#endif

	// Create epoll API
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if(epoll_fd == -1){
        perror("Server: Unable to create ePoll API.");
        // Terminate server process
        exit(EXIT_FAILURE);
    }

	pthread_t epoll_thread;
	// Set up epoll API to handle read/writes
	if(pthread_create(&epoll_thread, NULL, handle_epoll, NULL) != 0){
		// Error setting up epoll to handle clients
		perror("Server: Unable to set-up ePoll API.");
		// Terminate server process
		exit(EXIT_FAILURE);
	}

	// Continue to accept new clients
	#ifdef DEBUG
		printf("Server waiting ...\n");
	#endif

	// Begin listening on the socket
	while(1){
		// Important!! --> Server blocks on accept() call.
		if((client_sockfd = accept4(server_sockfd, (struct sockaddr *) NULL, NULL, SOCK_CLOEXEC)) != -1){
			// Close the file descriptor on exec
			fcntl(client_sockfd, F_SETFD, FD_CLOEXEC);

			// Notify server and continue listening for new clients
			#ifdef DEBUG
				printf("Processing new client ...\n");
			#endif

			// A successful client should create a thread to handle the client and return the server to listening for another connection.
			// Start a thread to handle the client ONLY if new client was accepted

			// Use a thread to handle the client
			pthread_t client_thread;
			int *fd_ptr = malloc(sizeof(int));
			*fd_ptr = client_sockfd;
			if(pthread_create(&client_thread, NULL, handle_client, fd_ptr) != 0){
				perror("Server: Unable to create thread for handling a client.");
			}

			// Ignore client threads
			if(pthread_detach(client_thread) != 0){
				perror("Server: Unable to detach thread, Kernel still listening for it..");
			}

			// Continue to accept new clients
			#ifdef DEBUG
				printf("Server waiting ...\n");
			#endif
		}
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

	// Close the file descriptor on exec
	fcntl(server_sockfd, F_SETFD, FD_CLOEXEC);

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
void *handle_client(void *arg){
	// NOTE: This is performed by the main server loop to ensure that the thread is detached even if it fails before it is able to process these lines of code.

	// // Don't record thread status
	// if(pthread_detach(pthread_self()) != 0){
	// 	perror("Server: Unable to detach thread, Kernel still listening for it..");
	// }

	// Decode client socket file descriptor
	int connect_fd = *(int*)arg;
	free(arg);

	// Call protocol_exchange()
	// TODO: Add timers to limit protocol exchange
	if(protocol_exchange(connect_fd) == -1){
		fprintf(stderr, "Server: Unable to complete protocol exchange with client.");
		// Terminate server thread for handling client connections
		pthread_exit(NULL);
	}

	// Confirm new client and create a pseudoterminal
	#ifdef DEBUG
		printf("Client-Server Protocol Exchange Completed.\n");
		printf("Create PTY for client with file descriptor %d.\n", connect_fd);
	#endif

	// Master file descriptor for pseudoterminal
	int master_fd;

	// Need a loction for the pty_slave_name
	char pty_slave_name[MAX_SNAME];
	
	// Create pseudoterminal for bash to execute on
	if((master_fd = create_pty(pty_slave_name)) == -1){
		perror("Server: Unable to create PTY. Cancelled client connection.");
		// Terminate server thread handling client connection
		pthread_exit(NULL);
	}

	// Register client file descriptors in global scope
	epoll_fd_pairs[master_fd] = connect_fd;
	epoll_fd_pairs[connect_fd] = master_fd;

	#ifdef DEBUG
		printf("Master FD: %d\n", master_fd);
		printf("Connect FD: %d\n", connect_fd);
		printf("epoll_fd_pairs[%d]: %d\n", epoll_fd_pairs[master_fd],epoll_fd_pairs[connect_fd]);
		printf("epoll_fd_pairs[%d]: %d\n", epoll_fd_pairs[connect_fd],epoll_fd_pairs[master_fd]);
	#endif

	ev.events = EPOLLIN;
	ev.data.fd = connect_fd;
	// Add client file descriptors to epoll	
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connect_fd, &ev) == -1){
	    perror("Server: Unable to add to ePoll interest list.");
		// Terminate server thread handling client connection
		pthread_exit(NULL);
	}

	ev.events = EPOLLIN;
	ev.data.fd = master_fd;
	// Add server file descriptor to epoll
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, master_fd, &ev) == -1){
	    perror("Server: Unable to add to ePoll interest list.");
		// Terminate server thread handling client connection
		pthread_exit(NULL);
	}

	#ifdef DEBUG
		printf("PTY Created.  Start sub-process for Bash to be executed in.\n");
	#endif

	// Create a sub-process to handle Bash on a pseudoterminal
	switch(cpid[0] = fork()){
		case -1:
			// Check if fork failed, still in thread for server to handle client
			perror("Server: Unable to create sub-process for Bash to execute in.");
			// If I can't fork, terminate the server thread
			pthread_exit(NULL);
		case 0:
			// This is creating a sub-process with inheritance of file descriptors

			#ifdef DEBUG
				printf("Server sub-process for Bash.\n");
			#endif

			// The PTY Master file descriptor is not needed in the server sub-process
			close(master_fd);

			run_pty_shell(pty_slave_name, connect_fd);

			// If Bash failed, terminate server sub-process, file descriptors get erased on exit
			exit(EXIT_FAILURE);			
	}

	// Bash executing in new sub-process with pseudoterminal
	// Return of control flow to server thread

	// // Read from bash
	// // TODO: Move this to epoll
 //   	int nread, nwrite, total;
 //   	char from_bash[BUFFER_SIZE];
	// nwrite = 0;
	// while(nwrite != -1 && (nread = read(master_fd,from_bash,BUFFER_SIZE)) > 0){
	// 	total = 0;
	// 	do{
	// 		// Write to client
	// 		if((nwrite = write(connect_fd,from_bash+total,nread-total)) == -1){
	// 			break;
	// 		}
	// 		total += nwrite;
	// 		// Keep reading from the buffer until it is done
	// 	}while(total < nread);
	// }
	// if(errno){
	// 	// Server secondary sub-process error reading from buffer
	// 	perror("Server: Unable to read from Bash output.");
	// }

	// // No more data to transfer
	// // Need to remove SIGCHLD handler and set to ignore so it wont collect
	// // TODO: I think I still need this here to catch when Bash terminates
	// // act.sa_handler = SIG_IGN;
	// // if(sigaction(SIGCHLD,&act,NULL) == -1){
	// // 	perror("Server: Error ignoring SIGCHLD.");
	// // }

	// // Terminate child processes
	// kill(cpid[0],SIGTERM);

	// // Collect any remaining child processes, this blocks until bash is done being used.
	// while (waitpid(-1,NULL,WNOHANG) > 0);

	// Terminate server thread for handling client connection
	pthread_exit(NULL);
}
// End of handle_client()


// Function to handle ePoll API
void *handle_epoll(){
	#ifdef DEBUG
		printf("Server ePoll API started.\n");
	#endif
	
    char buffer[BUFFER_SIZE];
   	int nread, nwrite, total;

    // Find file desciptors with needs
    while(1){
    	// This will print infinite times!
    	// #ifdef DEBUG
	    //     printf("Starting ePoll event tracking loop.\n");
	    // #endif


    	// Set event type
    	ev.events = EPOLLIN;

	    // Create epoll interest list from global variable
	    // Calculate how many file descriptors are ready
	    int ready_fds;
        ready_fds = epoll_wait(epoll_fd, evlist, MAX_EVENTS, -1);
        if (ready_fds == -1) {
            if (errno == EINTR)
                continue;               /* Restart if interrupted by signal */
            else{
        		perror("Server: Problem with ePoll interest list.");
    		    // Terminate server process
		        exit(EXIT_FAILURE);
            }
        }

		// This will print infinite times!
	    // #ifdef DEBUG
	    //     printf("Need to Handle: %d\n", ready_fds);
	    // #endif

	    int read_from_socket, write_to_socket = 0;
	    // Process ready file descriptors
        for (int j = 5; j < ready_fds; j++){
            if(evlist[j].events & EPOLLIN){
            	// Determine where to read from and where to write to
            	read_from_socket = epoll_fd_pairs[evlist[j].data.fd];
            	write_to_socket = epoll_fd_pairs[read_from_socket];

            	// Handle read/write
				nwrite = 0;
				while(nwrite != -1 && (nread = read(read_from_socket,buffer,BUFFER_SIZE)) > 0){
					total = 0;
					do{
						// Write to client
						if((nwrite = write(write_to_socket,buffer+total,nread-total)) == -1){
							break;
						}
						total += nwrite;
						// Keep reading from the buffer until it is done
					}while(total < nread);
				}
				if(errno){
					// Server secondary sub-process error reading from buffer
					perror("Server: Unable to read from Bash output.");
		   		    // Terminate server process
			        exit(EXIT_FAILURE);
				}

			}

        }
    }

    #ifdef DEBUG
	    printf("All file descriptors closed.\n");	
	#endif

	// Terminate ePoll API
	pthread_exit(NULL);
}
// End of handle_epoll()


// Called by handle_client() for protocol exchange with server
// Returns 0 on successful protocol exchange
int protocol_exchange(int connect_fd){
	// Start a timer to limit protocol exchange
	struct itimerspec ts;
	struct sigaction sa;
	struct sigevent sevp;
	timer_t *timer_id;

	// Timer need only expire once
	ts.it_interval.tv_sec = 0;
	ts.it_interval.tv_nsec = 0;
	// Timer should be 1 second
	ts.it_value.tv_sec = time ( NULL) + 1;
    ts.it_value.tv_nsec = 0;

	// Sends signal to terminate if it exceeds the timer limit
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = signal_handler;
	sigemptyset(&sa.sa_mask);
	if(sigaction(SIGRTMAX, &sa, NULL) == -1){
		perror("Server: Unable to handle signal from timer expiration.");
		return -1;
	}

    // Notify by thread
	sevp.sigev_notify = SIGEV_THREAD_ID;
	sevp.sigev_notify_attributes = NULL;
	sevp.sigev_signo = SIGRTMAX;
	// Tried to set to a negative here
	// sev.sigev_signo = -1;
	sevp.sigev_value.sival_ptr = &timer_id;

	// This compiles, but the man pages say it should be "sev.sigev_notify_thread_id ="
	sevp._sigev_un._tid = pthread_self();
	// sevp.sigev_notify_thread_id = pthread_self();
	
	// Used for debugging
	sevp.sigev_value.sival_int = pthread_self();

    // TODO: Solve why timer is invalid
    if(timer_create(CLOCKID, &sevp, &timer_id) == -1){
    	if(errno == EINVAL){
    		// This is the error I get ... 
    		perror("Clock ID");
    	}
    	else if (errno == ENOMEM){
    		perror("Could not allocate memory");
    	}
    	else if (errno == EAGAIN){
    		perror("Kernel Allocation");
    	}
    	else{
	    	perror("Server: Unable to create a timer for protocol exchange.");
	    }
    	// End client cycle instead of server
    	return -1;
    }

    if(timer_settime(timer_id, 0, &ts, NULL) == -1){
       	perror("Server: Unable to start timer for protocol exchange.");
    	// End client cycle instead of server
    	return -1;	
    }


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


// Called by handle_client() to run PTY shell [This is where BASH executes!]
void run_pty_shell(char *pty_slave_name, int connect_fd){
	// Child is the leader of the new session and looses its controlling terminal.
	if(setsid() == -1){
		perror("Server: Unable to set a new session.");
		// Terminate server sub-process
		exit(EXIT_FAILURE);
	}

	// Set PTY slave as the controlling terminal
	int slave_fd;
	if((slave_fd = open(pty_slave_name, O_RDWR)) == -1){
		perror("Server: Unable to set controlling terminal.");
		// Terminate server sub-process
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
	// Terminate server sub-process
	exit(EXIT_FAILURE);
}
// End of run_pty_shell()


// Handle signals during protocol exchange
void signal_handler(int sig, siginfo_t *si, void *uc){
	#ifdef DEBUG
		printf("Signal Called!");
	#endif

	// Identify which timer to handle
	timer_t *timer_id;
	timer_id = si->si_value.sival_ptr;
	// Delete the timer
	if(timer_delete(&timer_id) == -1){
		perror("Server: Problem deleting a timer.");
	}

	// Terminate server thread
	#ifdef DEBUG
		// Identify which thread is being terminated
		pid_t tid;
		tid = si->si_value.sival_int;
		printf("Terminate Thread: %d", tid);
	#endif

	// Terminate this thread
	pthread_exit(NULL);

	#ifdef DEBUG
		printf("Thread Terminated");
	#endif
}
// End of signal_handler()