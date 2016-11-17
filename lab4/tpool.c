/* Kirk Powell
 * CS 407 - Lab 4
 * November 4, 2016
 * 
 * Compile Using Debug:
 * $ gcc -Wall -Wextra -Werror -pedantic tpool-test.c -o tpool-test.exe -pthread
 * Otherwise, compile:
 * $ gcc -o tpool-test.exe tpool-test.c -pthread
 *
 * Sources:
 	CS407 Lab Solutions by NJ Carver
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

// Create global variable struct type for jobs in queue
struct jobs_object{
	// Set pointer to the next job in queue. Set to NULL if last/only job
	struct jobs_object *next_job;
	// Store file descriptor
	int job_id;
};

// Create global variable struct type for job queue for workers to access
struct queue_object{
	// Keep track of how many jobs in the queue
	int jobs_available;
	// Keep track of linked list of jobs
	struct jobs_object *current_job;
	struct jobs_object *latest_job;
};

// Keep track of a mutex for read/write privilege on the queue
pthread_mutex_t rwlock = PTHREAD_MUTEX_INITIALIZER;
// Keep track of a mutex to prevent busy-waiting
pthread_mutex_t process_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_count = PTHREAD_COND_INITIALIZER;

// Create global variable struct type for thread pool object
struct tpool_object{
	// Function for referencing in worker threads
	void (*call_this_function)(int);
};


// Set maximum number of tasks per thread in job queue
#define INIT_TASKS_PER_THREAD 4

int create_worker_thread(int pool_index);
int destroy_thread_pool_resources();
void *my_little_worker_bee();
int tpool_add_task(int newtask);
int tpool_init(void (*process_task)(int));

// Global variables for the thread pool
struct queue_object jobs_queue;
struct tpool_object tpool;

/* --------------------------------------------------------------------------------------*/
// Functions for tpool.c


// Called by tpool_init() to create the worker threads
// Accepts a pool index identifier
// Returns 0 on success, -1 on failure
int create_worker_thread(int pool_index){
	// Create a worker thread object
	#ifdef DEBUG
		printf("Creating worker thread #%d.\n", pool_index + 1);
	#endif

	// Create the thread
	pthread_t worker_thread;
	if(pthread_create(&worker_thread, NULL, my_little_worker_bee, NULL) != 0){
		perror("Failure to create a worker thread.\n");
		return -1;	
	}
	pthread_detach(worker_thread);

	#ifdef DEBUG
		printf("Worker thread #%d created.\n", pool_index + 1);
	#endif

	// Worker thread details recorded
	return 0;
}
// End create_worker_thread


// Called by main to destroy thread pool
// Returns 0 on success, -1 on failure
int destroy_thread_pool_resources(){
	// This was not specified in the instructions.

	// TODO: Check if it exists?

	// TODO: Clear jobs

	// TODO: Clear jobs_queue

	// TODO: Clear threads

	// TODO: Clear thread pool

	// TODO: Clear mutexes

	// Thread pool destroyed
	#ifdef DEBUG
		printf("Thread pool destroyed.\n");
	#endif

	return 0;
}
// End of destroy_thread_poo_resources()


// Called by create_worker_thread()
// Returns 0 on success, -1 on failure
void *my_little_worker_bee(){
	// Identify this thread
	#ifdef DEBUG
		printf("Worker %ld is ready to process a task.\n", syscall(SYS_gettid));
	#endif

	// Worker should attempt to do a job
	
	// Find the the next task in the job queue
	// Infinite loop, should always be looking for a new task
	while(1){
		// Get next task, BLOCK until jobs_queue object is available to read from
		// Add a condition variable and mutex here to avoid busy waiting
		pthread_mutex_lock(&process_lock);
		while(jobs_queue.jobs_available == 0){
			pthread_cond_wait(&job_count, &process_lock);
		}
		// Remove from job queue
		jobs_queue.jobs_available--;
		pthread_mutex_unlock(&process_lock);

		// Use a mutex to access the job queue
		pthread_mutex_lock(&rwlock);
		
		// Get file descriptor for next task
		int file_descriptor = jobs_queue.current_job->job_id;
		// Show task being handled by a worker thread
		#ifdef DEBUG
			printf("Worker %ld is processing task #%d.\n", syscall(SYS_gettid),file_descriptor);
		#endif
		
		// Move to next task on linked list
		if((jobs_queue.current_job->next_job) != NULL){
			jobs_queue.current_job = jobs_queue.current_job->next_job;
			// TODO: Do I need to deallocate memory for the completed task?  Yes.  Slab allocation?

		}
		// If this was the last job in the queue, it doesn't matter for the worker thread.  It will continue in an infinite loop.


		// Track remaining jobs for testing
		#ifdef DEBUG
			int available_jobs = jobs_queue.jobs_available;
		#endif

		// Check for remaining jobs in queue, send a signal
		// TODO: This may be unnecessary
		// if(jobs_queue.jobs_available > 0){
		// 	pthread_cond_broadcast(&job_count);
		// }

		// Unlock the job queue
		pthread_mutex_unlock(&rwlock);

		// Call the function to handle the client file descriptor
		// There is no error check for this
		tpool.call_this_function(file_descriptor);

		// Acknowledge task completed
		#ifdef DEBUG
			// Pretend to do something for 5 seconds
			sleep(5);
			printf("Task #%d completed.\n", file_descriptor);
			printf("Tasks Remaining %d.\n", available_jobs);
		#endif
	}
	
	// Should never get here
	exit(EXIT_FAILURE);
}
// End of my_little_worker_bee()


// Called by main to add tasks to the job queue
// Accepts an integer (intended to be a file descriptor)
// Returns 0 on success, -1 on failure
int tpool_add_task(int newtask){
	#ifdef DEBUG
		printf("------------------------------------------------\n");
		printf("Creating task #%d.\n", newtask);
	#endif

	// Lock the job queue.  BLOCK until able to do so.
	pthread_mutex_lock(&rwlock);

	// Create a new object for a job
	struct jobs_object *new_task;
	if((new_task = malloc(sizeof(struct jobs_object))) == NULL){
		fprintf(stderr, "Problem adding task #%d to the job queue.\n", newtask);
		return -1;
	}
	// Record file descriptor
	new_task->job_id = newtask;
	// Set pointer for next job in linked list to null, tasks always placed at the end of the list
	new_task->next_job = NULL;
	#ifdef DEBUG
		printf("Task created for file descriptor #%d.\n", new_task->job_id);
	#endif

	// Add job to the queue
	if(jobs_queue.jobs_available != 0){
		#ifdef DEBUG
			printf("Add to existing queue.\n");
		#endif
		// There is more than one job in the queue
		// Set pointer for linked list to next job
		jobs_queue.latest_job->next_job = new_task;
		// Add to the end of the linked list
		jobs_queue.latest_job = new_task;
	}
	else{
		#ifdef DEBUG
			printf("Start new queue.\n");
		#endif
		// There are no jobs in the queue
		// Start a linked list, place task at the front
		jobs_queue.current_job = new_task;
		// Set task as end of the linked list
		jobs_queue.latest_job = new_task;
	}

	// Unlock the job queue
	pthread_mutex_unlock(&rwlock);

	// Use a mutex to lock job count
	pthread_mutex_lock(&process_lock);
	// Increment jobs available
	jobs_queue.jobs_available++;
	pthread_mutex_unlock(&process_lock);
	// Send a signal to condition variable that there is a job in the queue to process
	pthread_cond_signal(&job_count);

	#ifdef DEBUG
		printf("------------------------------------------------\n");
		printf("Task #%d inserted into job queue.\n", newtask);
		printf("Verify tasks in the job queue: %d jobs available.\n", jobs_queue.jobs_available);
		printf("Current Job: %d\n", jobs_queue.current_job->job_id);
		printf("Latest Job: %d\n", jobs_queue.latest_job->job_id);
		printf("------------------------------------------------\n");
	#endif

	// Task added to job que
	return 0;
}
// End of tpool_add_task()


// Begin tpool_init()
// Accepts a function as an argument.  This is the pointer for the worker to handle tasks
// Returns 0 on success, -1 on failure
int tpool_init(void (*process_task)(int)){
	#ifdef DEBUG
		printf("Initialize thread pool.\n");
	#endif

	// Use a system call to determine the number of CPU cores available for thread pool size
	long number_of_available_processing_cores = -1;
	// Determine number of available CPU cores
	if((number_of_available_processing_cores = sysconf(_SC_NPROCESSORS_ONLN)) == -1){
		fprintf(stderr, "Can't determine number of available CPU cores to set thread pool size.\n");
		return -1;
	}
	// Assign to number of threads
	// This is not necessary but makes the code more readable
	#define MAX_THREADS number_of_available_processing_cores 
	// Set maximum number of jobs in queue (available threads * tasks per thread + 1)
	// TODO: Do something with this?
	#define MAX_JOBS MAX_THREADS * INIT_TASKS_PER_THREAD + 1
	#ifdef DEBUG
		printf("Maximum threads available: %ld\n", MAX_THREADS);
		printf("Maximum jobs in queue: %ld\n", MAX_JOBS);
	#endif

	// Store the function to be used by workers
	tpool.call_this_function = process_task;
	#ifdef DEBUG
		printf("Thread pool started.\n");
	#endif

	#ifdef DEBUG
		printf("Creating a job queue for tasks.\n");
	#endif

	// Initialize with zero jobs
	jobs_queue.jobs_available = 0;
	jobs_queue.current_job = NULL;
	jobs_queue.latest_job = NULL;
	#ifdef DEBUG
		printf("Job queue created.\n");
		printf("Jobs Available: %d\n", jobs_queue.jobs_available);
	#endif

	#ifdef DEBUG
		printf("Create a place to track worker threads in the pool.\n");
	#endif

	#ifdef DEBUG
		printf("Create worker threads.\n");
	#endif
	for(int i = 0; i < MAX_THREADS; i++){
		// Create a worker thread and assign to pool index
		if((create_worker_thread(i)) == -1){
			fprintf(stderr, "Unable to create worker threads.\n");
			return -1;		
		}
		printf("Number of Worker Threads Created: %d\n", i + 1);
	}
	#ifdef DEBUG
		printf("Worker threads working ...\n");
	#endif
	// Thread pool up and running, return control to main to aggregate jobs to workers
	return 0;
}