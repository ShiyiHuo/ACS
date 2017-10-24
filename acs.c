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
#define MAX_CUSTOMER 256
#define MAX_INPUT_SIZE 1024
#define NQUEUE 4
#define NCLERK 2

customer_info customer_list[MAX_CUSTOMER];  //list of customers
int clerk_info[2] = {0,1};  //list of clerks (clerkid)

struct timeval init_time; // record the simulation start time; No need to use mutex_lock when reading this variable since the value would not be changed by thread once the initial time was set.
double overall_waiting_time = 0.0; // used to add up the overall waiting time for all customers, every customer add their own waiting time to this variable, mutex_lock is necessary.
int queue_length[NQUEUE];// real-time queue length information; mutex_lock needed

int C = -1; // to check which clerk awakes customer

pthread_t customer_threads[MAX_CUSTOMER];
pthread_t clerk_threads[2];

// Mutexes and convars for queues
pthread_mutex_t mutex_queue[NQUEUE];
pthread_cond_t convar_queue[NQUEUE];

// Mutexes and convars for clerks
pthread_mutex_t mutex_clerk[NCLERK];
pthread_cond_t convar_clerk[NCLERK];


// returns time difference (s) between cur_time and start_time
float getTimeDifference() {
	struct timeval nowTime;
	gettimeofday(&nowTime, NULL);
	long nowMicroseconds = (nowTime.tv_sec * 100000) + nowTime.tv_usec;
	long startMicroseconds = (init_time.tv_sec * 100000) + init_time.tv_usec;
	return (float)(nowMicroseconds - startMicroseconds) / 100000;
}


// function entry for customer threads
void * customer_entry(void * cus_info){
	// a new customer comes in
	struct customer_info * p_myInfo = (struct customer_info *) cus_info;

	usleep((p_myInfo->arrival_time)*100000); //sleep as long as arrival time

	fprintf(stdout, "A customer arrives: customer ID %2d. \n", p_myInfo->user_id);

	/* pick the shortest queue to enter into */
  int i;
  int min_queue_length = MAX_CUSTOMER+1;
  int shortest_queue;
  for (i = 0; i < 4; i++) {
    if (queue_length[i] < min_queue_length) {
      min_queue_length = queue_length[i];
      shortest_queue = i;
    }
  }

	pthread_mutex_lock(&mutex_queue[shortest_queue]);
  queue_length[shortest_queue]++;
	fprintf(stdout, "A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", shortest_queue, queue_length[shortest_queue]);
	/* insert customer into queue */

  int clerk;
	// customers are waiting for a clerk
	pthread_cond_wait(&convar_queue[shortest_queue], &mutex_queue[shortest_queue]);
	//Now pthread_cond_wait returned, customer was awaked by one of the clerks
  /* Try to figure out which clerk awaked me, because you need to print the clerk Id information */
  if (C == 0) { //clerk 0 awakes me
    clerk = 0;
    C = -1;
  } else if (C == 1) {  //clerk 1 awakes me
    clerk = 1;
    C = -1;
  }

	pthread_mutex_unlock(&mutex_queue[shortest_queue]); //unlock mutex_lock such that other customers can enter into the queue

	/* get the current machine time; updates the overall_waiting_time*/
	fprintf(stdout, "A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", getTimeDifference(), p_myInfo->user_id, clerk);

	usleep((p_myInfo->service_time)*1000000);

	/* get the current machine time; */
	fprintf(stdout, "A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", getTimeDifference(), p_myInfo->user_id, clerk);
  pthread_mutex_lock(&mutex_clerk[clerk]);
	pthread_cond_signal(&convar_clerk[clerk]); //Notice the clerk that service is finished, it can serve another customer
  pthread_mutex_unlock(&mutex_clerk[clerk]);

	pthread_exit(NULL);

	return NULL;
}


//function entry for clerk threads
void *clerk_entry(void * clerkNum){

	while(TRUE){
		// clerk is idle now

		/* Check if there are customers waiting in queues, if so, pick the queue with the longest customers. */
    int i;
    int max_queue_length = -1;
    int longest_queue;
    for (i = 0; i < 4; i++) {
      if (queue_length[i] > max_queue_length) {
        max_queue_length = queue_length[i];
        longest_queue = i;
      }
    }

    if (max_queue_length == 0) {
      usleep(10);
      continue;
    } else {  // max_queue_length > 0
  		pthread_mutex_lock(&mutex_queue[longest_queue]);
  		/* updates the queue_length, mutex_lock needed */
      queue_length[longest_queue]--;

  		/* remove customer from queue */
      //remove customer by calling cond_signal???

  		// record clerk id using global var
      if (C == -1) {
        C = *((int*)clerkNum);  //cast void pointer to int
        // Awake the customer (the one enter into the queue first) from the longest queue (notice the customer he can get served now)
    		pthread_cond_signal(&convar_queue[longest_queue]);
      } else {
        usleep(10);
        continue;
      }

  		// unlock mutex so that other customers can get in
  		pthread_mutex_unlock(&mutex_queue[longest_queue]);


  		pthread_mutex_lock(&mutex_queue[longest_queue]);
      // wait the customer to finish its service, clerk busy
  		pthread_cond_wait(&convar_queue[longest_queue], &mutex_queue[longest_queue]);
  		pthread_mutex_unlock(&mutex_queue[longest_queue]);
    }
	}

	pthread_exit(NULL);

	return NULL;
}



int main(int argc, char* argv[]) {
	if (argc != 2) {
		printf("Usage: ACS <input file>\n");
		exit(1);
	}

  int i;
	/** Read customer information from txt file and store them in the structure you created
		1. Allocate memory(array, link list etc.) to store the customer information.
		2. File operation: fopen fread getline/gets/fread ..., store information in data structure you created
	*/
  // TODO: need modification!
  int NCustomers = 0;
  FILE *fp = fopen(argv[1],"r");// read fild
	if(fp==NULL){
		printf("failed in reading file\n");
		return -1;
	}
	if(!(fscanf(fp,"%d\n",&NCustomers))){
		printf("check your input file, the first line is the number of flows");
		return -1;
	}

	int id,arrival,service;
	for(i = 0; i < NCustomers; i++) {
    // TODO: sanity check - data type and negative numbers
    if(!(fscanf(fp,"%d:%d,%d\n",&id,&arrival,&service))){
	     printf("Error: input not in format of 'id:arrival time,service time'");
	  }
    // if (id<0 || arrival<0 || service<0) {
    //
    // }
    customer_list[i].user_id=id;
	  customer_list[i].arrival_time=arrival;
	  customer_list[i].service_time=service;
  }
  fclose(fp); // release file descriptor

  // for(i = 0; i < NCustomers; i++) {
  //   printf("%d:%d,%d\n", customer_list[i].user_id, customer_list[i].arrival_time, customer_list[i].service_time);
  // }



  //initialize all the condition variable and thread lock will be used
  for (i = 0; i < 4; i++) {
    pthread_mutex_init(&mutex_queue[i], NULL);
    pthread_cond_init(&convar_queue[i], NULL);
  }

  for (i = 0; i < 2; i++) {
    pthread_mutex_init(&mutex_clerk[i], NULL);
    pthread_cond_init(&convar_clerk[i], NULL);
  }

  //get global start_time
  gettimeofday(&init_time,NULL);

	//create clerk thread
	for(i = 0; i < 2; i++){ // number of clerks
    // clerk_info: passing the clerk information (e.g., clerk ID) to clerk thread
		pthread_create(&clerk_threads[i], NULL, clerk_entry, (void *)&clerk_info[i]);
	}

	//create customer thread
	for(i = 0; i < NCustomers; i++) { // number of customers
    //custom_info: passing the customer information (e.g., customer ID, arrival time, service time, etc.) to customer thread
		pthread_create(&customer_threads[i], NULL, customer_entry, (void *)&customer_list[i]);
	}

	// wait for all customer threads to terminate
	for(i = 0; i < NCustomers; i++) { // number of customers
		pthread_join(customer_threads[i], NULL);
	}

	// destroy mutex & condition variable (optional)
	for (i = 0; i < 4; i++) {
		pthread_mutex_destroy(&mutex_queue[i]);
		pthread_cond_destroy(&convar_queue[i]);
	}

	for (i = 0; i < 2; i++) {
		pthread_mutex_destroy(&mutex_clerk[i]);
		pthread_cond_destroy(&convar_clerk[i]);
	}

	//calculate the average waiting time of all customers
  // overall_waiting_time = getTimeDifference();
  // double avg_waiting_time = overall_waiting_time/NCustomers;
  // fprintf(stdout, "The average waiting time for all customers in the system is %.2f seconds. \n", avg_waiting_time);




	return 0;
}
