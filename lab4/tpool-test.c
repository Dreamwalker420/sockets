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

// #include <errno.h>
// #include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// TODO: create a header file for thread pools 
#include "tpool.c"

int process_task();

// Begin main()
int main (){
	printf("Thread Pool Example.\n");

	// Call tpool_init() to create a thread pool
	// Pass a function as an argument, verify that this is working correctly
	if(tpool_init((void *)process_task) == -1){
		fprintf(stderr, "Unable to initialize a thread pool.\n");
		exit(EXIT_FAILURE);
	}

	// TODO: Display worker thread information

	printf("Add 40 tasks to the job queue.\n");
	// Add 40 tasks to the queue
	for(int i=0;i<40;i++){
		// Add a task to the job queue
		// Call tpool_add_task() to add a task to job queue
		if(tpool_add_task(i) == -1){
			fprintf(stderr, "Adding task #%d failed.\n", i);
		}
	}

	// TODO: Show jobs added to the queue

	// Process all the jobs in the queue
	while(jobs_queue.jobs_available > 0){

	}

	// No more jobs in queue, end thread pool
	// TODO: Destroy thread pool upon job completion
	if(destroy_thread_pool_resources() != 0){
		fprintf(stderr, "Problem destroying the thread pool resources.\n");
	}

	exit(EXIT_SUCCESS);
}
// End of main()


// Called by create_worker_thread() to handle a task available in the job queue
// Accepts an integer intended to be a file descriptor
// Return 0 on success, -1 on failure
int process_task(){
	// Identify this thread
	#ifdef DEBUG
		printf("Worker %ld is trying to process a task.\n", syscall(SYS_gettid));
	#endif

	// Worker should attempt to do a job
	
	// Pointer to next job to be processed
	void *process_this_job;
	// Find the the next task in the job queue
	// Infinite loop, should always be looking for a new task
	while(1){
		// Get next task, BLOCK until jobs_queue object is available to read from
		// TODO: Need to use a mutex to access the job queue

		// Check if there is a job available
		if(jobs_queue.jobs_available > 0){
			// Get pointer to next task
			process_this_job = jobs_queue.next_job;

			// Show task being handled by a worker thread
			// printf("Worker (#%ld) attempting to complete job #%d\n", syscall(SYS_gettid), process_this_job.job_id);
			int file_descriptor = 0;
			// Pretend to do something for 15 seconds
			sleep(15);

			// Acknowledge task completed
			printf("Task #%d completed.\n", file_descriptor);
		}
		else{
			// If there are no jobs available, end the thread and exit
			pthread_exit(NULL);
			break;
		}
	}

	// TODO: Should this be exit success here instead?
	return 0;
}
// End of process_task