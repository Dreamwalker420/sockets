/* Kirk Powell
 * CS 407 - Lab 4
 * November 4, 2016
 * 
 * Compile Using this format:
 * $ gcc -Wall tpool-test.c -o tpool-test.exe -pthread
 *
 *
 * Sources:
 	CS407 Lab Solutions by NJ Carver
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

// TODO: Turn this off when submitting for grading
#define DEBUG 1

// TODO: Create global variable struct type

#include <stdio.h>
// TODO: create a header file for thread pools


// TODO: Create global variable struct to hold tpool object data
// Must include: task queue components, mutexes and condition variables, and the function process_task()

// Set maximum number of tasks per thread
// TODO: Determine core size and set to the number of worker threads available + 1
#define INIT_TASKS_PER_THREAD 1000

int tpool_init();

int main (){
	printf("Thread Pool Test.\n");

	// TODO: Call tpool_init() to create a thread pool
	if(tpool_init() == -1){
		fprintf(stderr, "Unable to initialize a thread pool.\n");
		exit(EXIT_FAILURE);
	}
	// TODO: Pass function as an argument, this is the function used by worker threads
	// TODO: Include a job queue

	// TODO: Display current thread information

	// TODO: Add tasks to the que
	// TODO: Call tpool_add_task() to add a task to job queue

	// TODO: Show tasks have been added to the queue

	// TODO: Show task being handled by a worker thread
	exit(EXIT_SUCCESS);
}
// End of main


// Functions

// Called by main to create a thread pool
// Returns 0 on success, -1 on failure
int tpool_init(){
	printf("Initialize Thread Pool\n");
	return 0;
}
// End tpool_init()