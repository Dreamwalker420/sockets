/* Kirk Powell
 * CS 407 - Lab 4
 * November 4, 2016
 *
 * Sources:
 	CS407 Lab Solutions by NJ Carver
	"The Linux Programming Interface" [2010] by Michael Kerrisk
 	"Beginning Linux Programming" [2008] by Matthew and Stone.
 *
 */

typedef struct queue_object queue_object_t;
typedef struct tpool_object tpool_object_t;

static int create_jobs_queue(int max_jobs);
static int create_worker_thread(int pool_index);
int destroy_thread_pool_resources();
static void *my_little_worker_bee();
int tpool_add_task(int newtask);
int tpool_init(void (*process_task)(int));