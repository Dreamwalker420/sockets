/*
	Compile using:
	$  gcc -Wall -Wextra -pedantic -o test.exe test-kirk-queue.c -pthread
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

// Use a struc to reference details pertaining to the queue
typedef struct queue_object {
	// Pointer to the buffer
	int *queue_buffer;
	// Track the size of the buffer
	size_t buffer_length;
	// Keep track if a job is available
	int jobs_available;
	// Head
	int current_job;
	// Tail
	int latest_job;
	// Mutex to rw from the queue
	pthread_mutex_t jq_mutex;
} queue_object_t;

// Globally define the queue for reference across functions
queue_object_t jobs_queue;

#define INITIAL_QUEUE_SIZE 5

int add_job(int file_descriptor);
void *handle_task();
void print_queue(char *message);

int main(){
	printf("Circular Queue\n");

	// Allocate memory for a queue based on the max size of the queue needed
	jobs_queue.queue_buffer = malloc(INITIAL_QUEUE_SIZE);

	// Set parameters for the queue
	jobs_queue.current_job = 0;
	jobs_queue.latest_job = 0;
	jobs_queue.jobs_available = 0;
	jobs_queue.buffer_length = INITIAL_QUEUE_SIZE;
	pthread_mutex_init(&(jobs_queue.jq_mutex), NULL);

	print_queue("Queue Initialized");

	// Add two threads to process tasks
	for(int i=0;i<2;i++){
		pthread_t worker;
		pthread_create(&worker,NULL,handle_task,NULL);
	}

	printf("Threads running ...\n");

	// Add jobs to the queue
	for(int file_descriptor = 40;file_descriptor<=80;file_descriptor++){
		if((add_job(file_descriptor)) == -1){
			printf("Job Queue is Full. Cannot insert %d into queue.\n", file_descriptor);
		}
	}

	while(1){
		print_queue("Jobs Added");
	}
}

int add_job(int file_descriptor){
	// Need a mutex to lock the queue when adding a job
	pthread_mutex_lock(&jobs_queue.jq_mutex);

	// Check if job queue is full
	if(jobs_queue.jobs_available > 0){
		// Increment tail
		jobs_queue.latest_job++;

		// Check that latest job doesn't overflow the queue boundry
		if((size_t)jobs_queue.latest_job > jobs_queue.buffer_length){
			// Check if I can wrap around queue
			if(jobs_queue.current_job != 0){
				// Wrap to the front of the queue
				jobs_queue.latest_job = 0;
				// Set latest task
				jobs_queue.queue_buffer[jobs_queue.latest_job] = file_descriptor;
				// Increment jobs available
				jobs_queue.jobs_available++;
				return 0;
			}
			else{
				// Reallocate the size of the queue
				jobs_queue.buffer_length = jobs_queue.buffer_length * 2;
				printf("New buffer size? %lu\n", jobs_queue.buffer_length);
				int *temp_buffer = realloc(jobs_queue.queue_buffer,sizeof(int)*jobs_queue.buffer_length);
				jobs_queue.queue_buffer = temp_buffer;
				printf ("Expanded the job queue.\n");
				if(jobs_queue.current_job >= jobs_queue.latest_job){
					// Move head to end of the buffer
					jobs_queue.current_job = jobs_queue.buffer_length - 1;
				}
			}
		}
	}
	else{
		// Jobs queue is empty
		// Reset job indexes
		jobs_queue.current_job = 0;
		jobs_queue.latest_job = 0;		
	}

	// Set latest task
	jobs_queue.queue_buffer[jobs_queue.latest_job] = file_descriptor;
	// Increment jobs available
	jobs_queue.jobs_available++;

	// Unlock the queue
	pthread_mutex_unlock(&jobs_queue.jq_mutex);
	printf("File descriptor %d inserted into queue.\n", file_descriptor);
	print_queue("Jobs Added");
	sleep(1);
	return 0;
}

void *handle_task(){
	int file_descriptor = 0;
	while(1){
		pthread_mutex_lock(&jobs_queue.jq_mutex);
		if(jobs_queue.jobs_available > 0){
			// Get file descriptor
			file_descriptor = jobs_queue.queue_buffer[jobs_queue.current_job];
			printf("Thread #%ld is handling file descriptor %d\n", syscall(SYS_gettid), file_descriptor);

			// Remove from jobs available
			jobs_queue.jobs_available--;
			// Check if current job exceeds boundary
			if((size_t)jobs_queue.current_job >= jobs_queue.buffer_length){
				// Wrap to start of queue
				jobs_queue.current_job = 0;
			}
			else if(jobs_queue.current_job >= jobs_queue.latest_job && jobs_queue.jobs_available == 0){
				// Jobs queue is empty
				// Reset job indexes
				jobs_queue.current_job = 0;
				jobs_queue.latest_job = 0;
			}
			else{
				// Move to next job on the queue
				jobs_queue.current_job++;
			}
		}
		pthread_mutex_unlock(&jobs_queue.jq_mutex);
		sleep(5);
	}

}

void print_queue(char *message){
	printf("----------------------------------------------------\n");
	printf("%s\n", message);
	printf("Jobs Available: %d\n", jobs_queue.jobs_available);
	printf("Current Job: %d\n", jobs_queue.current_job);
	printf("File Descriptor for Current Job: %d\n", jobs_queue.queue_buffer[jobs_queue.current_job]);
	printf("Latest Job: %d\n", jobs_queue.latest_job);
	printf("File Descriptor for Latest Job: %d\n", jobs_queue.queue_buffer[jobs_queue.latest_job]);
	printf("----------------------------------------------------\n");	
}