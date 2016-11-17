/*
	Compile using:
	$  gcc -Wall -Wextra -Werror -pedantic -o test.exe unbounded-queue.c -pthread

	Add execute as:
	$ ./test.exe

	Then compile using
	$ gcc -o name_of_executable unbounded-queue.c -pthread
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
	size_t current_job;
	// Tail
	size_t latest_job;
	// Mutex to rw from the queue
	pthread_mutex_t jq_mutex;
} queue_object_t;

// Globally define the queue for reference across functions
queue_object_t jobs_queue;

int add_job(int file_descriptor);
void *handle_task();
void print_queue(char *message);

int main(){
	printf("Circular Queue\n");

	// Allocate memory for a queue based on the max size of the queue needed
	jobs_queue.queue_buffer = malloc(sizeof(int)*5);

	// Set parameters for the queue
	jobs_queue.current_job = 0;
	jobs_queue.latest_job = 0;
	jobs_queue.queue_buffer[0] = -1;
	jobs_queue.jobs_available = 0;
	jobs_queue.buffer_length = 5;
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
}

int add_job(int file_descriptor){
	// Need a mutex to lock the queue when adding a job
	pthread_mutex_lock(&jobs_queue.jq_mutex);

	// Determine next position for a new job
	size_t new_job_at = jobs_queue.latest_job + 1;

	// Check if the buffer is full
	if((size_t)jobs_queue.jobs_available == jobs_queue.buffer_length){
		// Expand the queue
		// Track the current buffer size if needed
		size_t old_buffer_size = jobs_queue.buffer_length;
		jobs_queue.buffer_length = jobs_queue.buffer_length * 2;
		printf("New buffer size? %lu\n", jobs_queue.buffer_length);
		int *temp_buffer = realloc(jobs_queue.queue_buffer,sizeof(int)*jobs_queue.buffer_length);
		jobs_queue.queue_buffer = temp_buffer;
		printf ("Expanded the job queue.\n");

		// Check if current job is higher than latest job
		if(jobs_queue.current_job > jobs_queue.latest_job){
			// Check where current job is
			if(jobs_queue.current_job == (old_buffer_size - 1)){
				// Move current job to end of the new queue
				jobs_queue.queue_buffer[jobs_queue.buffer_length -1] = jobs_queue.queue_buffer[jobs_queue.current_job];
				jobs_queue.current_job = jobs_queue.buffer_length - 1;
			}
			else{
				// Move jobs to the new buffer space
				size_t new_position = old_buffer_size;
				for(size_t array_index = 0;array_index < jobs_queue.current_job; array_index++){
					new_position = old_buffer_size + array_index;
					jobs_queue.queue_buffer[new_position] = jobs_queue.queue_buffer[array_index];
					jobs_queue.queue_buffer[array_index] = -1;
				}
				new_job_at = new_position + 1;
			}
		}
	}
	else{
		if(jobs_queue.jobs_available == 0){
			// Reset jobs queue
			new_job_at = 0;
			jobs_queue.current_job = 0;
			jobs_queue.queue_buffer[0] = -1;
		}
		else{
			// Get new position for latest job
			new_job_at = ((jobs_queue.latest_job + 1) % jobs_queue.buffer_length);
		}
	}

	// Assign new job
	// Set latest task
	jobs_queue.latest_job = new_job_at;
	jobs_queue.queue_buffer[jobs_queue.latest_job] = file_descriptor;

	// Increment jobs available
	jobs_queue.jobs_available++;

	// Unlock the queue
	pthread_mutex_unlock(&jobs_queue.jq_mutex);
	print_queue("Job Added");
	printf("File descriptor %d inserted into queue.\n", file_descriptor);	
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
			jobs_queue.queue_buffer[jobs_queue.current_job] = -1;
			if(jobs_queue.current_job != jobs_queue.latest_job){			
				// Move to next job on the queue
				jobs_queue.current_job++;
				// Check if current job exceeds boundary
				if(jobs_queue.current_job > jobs_queue.buffer_length - 1){
					// Wrap to start of queue
					jobs_queue.current_job = 0;
				}
			}
		}
		printf("Jobs Available: %d\n", jobs_queue.jobs_available);
		pthread_mutex_unlock(&jobs_queue.jq_mutex);
		sleep(7);
	}
	return 0;
}

void print_queue(char *message){
	printf("----------------------------------------------------\n");
	printf("%s\n", message);
	printf("Jobs Available: %d\n", jobs_queue.jobs_available);
	printf("Current Job: %lu\t\t", jobs_queue.current_job);
	printf("Latest Job: %lu\n", jobs_queue.latest_job);
	printf("----------------------------------------------------\n");
	for(size_t index = 0;index < jobs_queue.buffer_length; index++){
		printf("[%lu]\t", index);
	}
	printf("\n");
	for(size_t content = 0;content < jobs_queue.buffer_length; content++){
		printf(" %d \t", jobs_queue.queue_buffer[content]);
	}
	printf("\n");
	printf("----------------------------------------------------\n");	
}