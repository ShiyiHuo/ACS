/**
 Hints on the basic logic of assignment 2, authored by Huan Wang
 
 The following design hints are based on my own implementation, you just take it as a reference. Do Not need to completely follow my design
 e.g., I do the service simulation(customer served by clerk) in customer thread(using usleep()), you can do it in clerk thread;
	   I create all the customer threads in one time, you can create these customer threads gradually according to the arrival time.
	   
 The time calculation could be confused, check the exmaple of gettimeofday on connex->resource->tutorial for more detail.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>


typedef struct customer_info{ /// use this struct to record the customer information read from customers.txt
    int user_id;
	int service_time;
	int arrival_time;
} customer_info;

/* global variables */
#define TRUE 1
#define FALSE 0
#define MAXFLOW 256
#define MAX_INPUT_SIZE 1024
#define NQUEUE 4
#define NCLERK 2


struct timeval init_time; // record the simulation start time; No need to use mutex_lock when reading this variable since the value would not be changed by thread once the initial time was set.
double overall_waiting_time; // used to add up the overall waiting time for all customers, every customer add their own waiting time to this variable, mutex_lock is necessary.
int queue_length[NQUEUE];// real-time queue length information; mutex_lock needed

// Mutexes and convars for queues
pthread_mutex_t mutex_queue[NQUEUE];
pthread_cond_t convar_queue[NQUEUE];

// Mutexes and convars for clerks
pthread_mutex_t mutex_clerk[NCLERK];
pthread_cond_t convar_clerk[NCLERK];

// return 0 for success and return 1 for failure
// int read_input_file(char* file_path, char* file_content[]) {
//   FILE* input_file = fopen(file_path, "r");

//   if (input_file != NULL) {
//   	int i = 0;
//     while (fgets(file_content[i], sizeof(input_file)-1, input_file) != NULL) {
//       i++;
//     }

//     fclose(input_file);
//     return 0;
//   } else {
//   	printf("Error: Cannot open input file\n");
//     return 1;
//   }
// }


int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Usage: ACS <input file>\n");
		exit(1);
	}

	// initialize all the condition variable and thread lock will be used
	int i;
	// for (i = 0; i < 4; i++) {
	// 	pthread_mutex_init(&mutex_queue[i], NULL);
	// 	pthread_cond_init(&convar_queue[i], NULL);
	// }
	
	// for (i = 0; i < 2; i++) {
	// 	pthread_mutex_init(&mutex_clerk[i], NULL);
	// 	pthread_cond_init(&convar_clerk[i], NULL);
	// }

	/** Read customer information from txt file and store them in the structure you created 
		1. Allocate memory(array, link list etc.) to store the customer information.
		2. File operation: fopen fread getline/gets/fread ..., store information in data structure you created
	*/
	char file_content[1024][1024];
    FILE* input_file = fopen(*argv[1], "r");
  	if (input_file != NULL) {
	  	int i = 0;
	    while (fgets(file_content[i], sizeof(input_file)-1, input_file) != NULL) {
	      i++;
	    }
    	fclose(input_file);
  	} else {
  		printf("Error: Cannot open input file\n");
  	}

  	for (i = 0; i < 100; i++) {
  		printf("%s", file_content[i]);
  	}

	//create clerk thread (optional)
	// for(i = 0, i < NClerks; i++){ // number of clerks
	// 	pthread_create(&clerkId[i], NULL, clerk_entry, (void *)&clerk_info[i]); // clerk_info: passing the clerk information (e.g., clerk ID) to clerk thread
	// }
	
	// //create customer thread
	// for(i = 0, i < NCustomers; i++){ // number of customers
	// 	pthread_create(&customId[i], NULL, customer_entry, (void *)&custom_info[i]); //custom_info: passing the customer information (e.g., customer ID, arrival time, service time, etc.) to customer thread
	// }

	// // wait for all customer threads to terminate
	// for(i = 0, i < NCustomers; i++){ // number of customers
	// 	pthread_join(&customId[i], NULL);
	// }

	// // destroy mutex & condition variable (optional)
	// for (i = 0; i < 4; i++) {
	// 	pthread_mutex_destroy(&mutex_queue[i]);
	// 	pthread_cond_destroy(&convar_queue[i]);
	// }

	// for (i = 0; i < 2; i++) {
	// 	pthread_mutex_destroy(&mutex_clerk[i]);
	// 	pthread_cond_destroy(&convar_clerk[i]);
	// }

	// calculate the average waiting time of all customers





	return 0;
}

// function entry for customer threads
// void * customer_entry(void * cus_info){
// 	// a new customer comes in
// 	struct customer_info * p_myInfo = (struct info_node *) cus_info;
	
// 	usleep(/* as long as the arrival time of this customer */);
	
// 	fprintf(stdout, "A customer arrives: customer ID %2d. \n", p_myInfo->user_id);
	
// 	/* pick the shortest queue to enter into */
	
// 	pthread_mutex_lock(/* mutexLock of selected queue */);
	
// 	fprintf(stdout, "A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", /*...*/);

// 	/* updates the queue_length, mutex_lock needed */
	
// 	/* insert customer into queue */

// 	// customers are waiting for a clerk
// 	pthread_cond_wait(/* cond_var of selected queue */, /* mutexLock of selected queue */);
// 	//Now pthread_cond_wait returned, customer was awaked by one of the clerks

// 	pthread_mutex_unlock(mutexLock of selected queue); //unlock mutex_lock such that other customers can enter into the queue
	
// 	/* Try to figure out which clerk awaked me, because you need to print the clerk Id information */
	
// 	/* get the current machine time; updates the overall_waiting_time*/
// 	// check sample_gettimeofday.c
	
// 	fprintf(stdout, "A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", /*...*/);
	
// 	usleep(/* as long as the service time of this customer */);
	
// 	/* get the current machine time; */
// 	fprintf(stdout, "A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", /* ... */);\
	
// 	pthread_cond_signal(/* The clerk awaked me */); //Notice the clerk that service is finished, it can serve another customer
	
// 	pthread_exit(NULL);
	
// 	return NULL;
// }

// function entry for clerk threads
// void *clerk_entry(void * clerkNum){
	
// 	while(TRUE){
// 		// clerk is idle now
		
// 		/* Check if there are customers waiting in queues, if so, pick the queue with the longest customers. */
		
// 		pthread_mutex_lock(/* mutexLock of the selected queue */);
// 		/* updates the queue_length, mutex_lock needed */
	
// 		/* remove customer from queue */

// 		// record clerk id using global var
// 		pthread_cond_signal(/* cond_var of the selected queue */); // Awake the customer (the one enter into the queue first) from the longest queue (notice the customer he can get served now)
// 		// unlock mutex so that other customers can get in
// 		pthread_mutex_unlock(/* mutexLock of the selected queue */);

		
// 		pthread_mutex_lock(/* mutexLock of the selected queue */);
// 		pthread_cond_wait(/* cond_var of selected queue */, /* mutexLock of selected queue */); // wait the customer to finish its service, clerk busy
// 		pthread_mutex_unlock(/* mutexLock of the selected queue */);
// 	}
	
// 	pthread_exit(NULL);
	
// 	return NULL;
// }
