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

int tpool_init(void (*process_task)(int));

int tpool_add_task(int newtask);