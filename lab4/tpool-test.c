/* Kirk Powell
 * CS 407 - Lab 4
 * November 4, 2016
 * 
 * Build library files using:
 * Create object file from C source code (tpool.o)
 * gcc -c tpool.c
 * Create static library from object file
 * ar -cr tpool.a tpool.o
 * Create position independent code for an object file for shared library
 * This requires a new tpool.o
 * gcc -c -fpic tpool.c
 * [gcc -c -fPIC tpool.c] may be necessry
 * gcc -shared -o tpool.so tpool.o
 *
 * Compile Using this format:
 * $ gcc -Wall tpool-test.c -o tpool-test.exe -pthread
 * $ gcc -o tpool-test.exe tpool-test.c -pthread -L/home/dreamwalker/Documents/School/CS407/lab4/tpool.so -I /home/dreamwalker/Documents/School/CS407/lab4
 *
 * Sources:
 	CS407 Lab Solutions by NJ Carver
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

// TODO: Turn this off when submitting for grading
#define DEBUG 1

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
//#include <tpool.h>
#include "tpool.c"

void test_process_task(int file_descriptor);

// Begin main()
int main (){
	printf("Thread Pool Example.\n");
	// Call tpool_init() to create a thread pool
	// Pass a function as an argument, verify that this is working correctly
	if(tpool_init(test_process_task) == -1){
		fprintf(stderr, "Unable to initialize a thread pool.\n");
		exit(EXIT_FAILURE);
	}

	printf("Add 40 tasks to the job queue.\n");
	// Add 40 tasks to the queue
	for(int i=1;i<=40;i++){
		// Add a task to the job queue
		// Call tpool_add_task() to add a task to job queue
		if(tpool_add_task(i) == -1){
			fprintf(stderr, "Adding task #%d failed.\n", i);
		}
	}

	// Process all the jobs in the queue
	while(1){

	}

	// No more jobs in queue, end thread pool
	// TODO: Destroy thread pool upon job completion
	// TODO: Move this to the tpool.c?
	// if(destroy_thread_pool_resources() != 0){
	// 	fprintf(stderr, "Problem destroying the thread pool resources.\n");
	// }

	// Sould never get here
	exit(EXIT_SUCCESS);
}
// End of main()


// Called by create_worker_thread() to handle a task available in the job queue
// Accepts an integer intended to be a file descriptor
// Return 0 on success, -1 on failure
void test_process_task(int file_descriptor){
	printf("Process file descriptor #%d\n", file_descriptor);
}
// End of process_task