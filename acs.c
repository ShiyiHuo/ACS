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
  double waiting_time_start;
  double waiting_time_end;
  double waiting_time_total;
} customer_info;

/* global variables */
#define TRUE 1
#define FALSE 0
#define MAX_CUSTOMER 256
#define MAX_INPUT_SIZE 1024
#define NQUEUE 4
#define NCLERK 2
#define TO_UTIME 1000000

customer_info customer_list[MAX_CUSTOMER];  //list of customers
int clerk_info[2] = {0,1};  //list of clerks (clerkid)

struct timeval init_time; // record the simulation start time; No need to use mutex_lock when reading this variable since the value would not be changed by thread once the initial time was set.
double overall_waiting_time; // used to add up the overall waiting time for all customers, every customer add their own waiting time to this variable, mutex_lock is necessary.

int queue_length[NQUEUE];// real-time queue length information; mutex_lock needed
int C = -1; // to check which clerk awakes customer
pthread_mutex_t mutex_queue_length;
pthread_mutex_t mutex_C;

pthread_t customer_threads[MAX_CUSTOMER];
pthread_t clerk_threads[2];

// Mutexes and convars for queues
pthread_mutex_t mutex_queue[NQUEUE];
pthread_cond_t convar_queue[NQUEUE];

// Mutexes and convars for clerks
pthread_mutex_t mutex_clerk[NCLERK];
pthread_cond_t convar_clerk[NCLERK];


// returns time difference (s) between cur_time and start_time
double getTimeDifference() {
  struct timeval cur_time;
	double cur_secs, init_secs;

	//pthread_mutex_lock(&start_time_mtex); you may need a lock here
	init_secs = (init_time.tv_sec + (double) init_time.tv_usec / TO_UTIME);
	//pthread_mutex_unlock(&start_time_mtex);

	gettimeofday(&cur_time, NULL);
	cur_secs = (cur_time.tv_sec + (double) cur_time.tv_usec / TO_UTIME);

	return cur_secs - init_secs;
}


// function entry for customer threads
void * customer_entry(void * cus_info){
	// a new customer comes in
	struct customer_info * p_myInfo = (struct customer_info *) cus_info;

	usleep((p_myInfo->arrival_time)*TO_UTIME/10); //sleep as long as arrival time

	fprintf(stdout, "A customer arrives: customer ID %2d. \n", p_myInfo->user_id);

  int i;
	/* pick the shortest queue to enter into */
  int min_queue_length = MAX_CUSTOMER+1;
  int shortest_queue;

  pthread_mutex_lock(&mutex_queue_length);
  for (i = 0; i < NQUEUE; i++) {
    //printf("CUSTOMER - queue length of queue %d is %d\n",i, queue_length[i]);
    if (queue_length[i] < min_queue_length) {
      min_queue_length = queue_length[i];
      shortest_queue = i;
    }
  }
  queue_length[shortest_queue]++;
  pthread_mutex_unlock(&mutex_queue_length);

	pthread_mutex_lock(&mutex_queue[shortest_queue]);

	fprintf(stdout, "A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", shortest_queue, queue_length[shortest_queue]);
  /* insert customer into queue */

  //printf("CUSTOMER - queue length after enqueue: %d\n", queue_length[shortest_queue]);
  p_myInfo->waiting_time_start = getTimeDifference();
	// customers are waiting for a clerk
	pthread_cond_wait(&convar_queue[shortest_queue], &mutex_queue[shortest_queue]);
	//Now pthread_cond_wait returned, customer was awaked by one of the clerks
  p_myInfo->waiting_time_end = getTimeDifference();
  p_myInfo->waiting_time_total = p_myInfo->waiting_time_end - p_myInfo->waiting_time_start;
	pthread_mutex_unlock(&mutex_queue[shortest_queue]); //unlock mutex_lock such that other customers can enter into the queue

  /* Try to figure out which clerk awaked me, because you need to print the clerk Id information */
  int clerk;
  pthread_mutex_lock(&mutex_C);
  if (C == 0) { //clerk 0 awakes me
    clerk = 0;
    C = -1;
  }
  if (C == 1) {  //clerk 1 awakes me
    clerk = 1;
    C = -1;
  }
  pthread_mutex_unlock(&mutex_C);

	/* get the current machine time; updates the overall_waiting_time*/
	fprintf(stdout, "A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", getTimeDifference(), p_myInfo->user_id, clerk);

	usleep((p_myInfo->service_time)*TO_UTIME/10);

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
    pthread_mutex_lock(&mutex_queue_length);
    for (i = 0; i < NQUEUE; i++) {
      //printf("CLERK - queue length of queue %d is %d\n",i, queue_length[i]);
      if (queue_length[i] > max_queue_length) {
        max_queue_length = queue_length[i];
        longest_queue = i;
      }
    }

    if (queue_length[longest_queue] == 0) {
      usleep(10);
      pthread_mutex_unlock(&mutex_queue_length);
      break;
    } else {  // max_queue_length > 0
      queue_length[longest_queue]--;
      pthread_mutex_unlock(&mutex_queue_length);

      pthread_mutex_lock(&mutex_queue[longest_queue]);
  		/* remove customer from queue */
  		// record clerk id using global var
      pthread_mutex_lock(&mutex_C);
      while (1) {
        if (C == -1) {
          C = *((int*)clerkNum);  //cast void pointer to int
          // Awake the customer (the one enter into the queue first) from the longest queue (notice the customer he can get served now)
      		pthread_cond_signal(&convar_queue[longest_queue]);
          break;
        } else {
          usleep(10);
        }
      }
      pthread_mutex_unlock(&mutex_C);

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

  // read in customer info, filter out records with negative values
  int NCustomers;
  int curr_customer = 0;
  FILE *fp = fopen(argv[1],"r");// read fild
	if(fp==NULL){
		printf("Error: Failed in reading file\n");
		return -1;
	}
	fscanf(fp,"%d\n",&NCustomers);
	int id,arrival,service;

  while (curr_customer<NCustomers) {
  	if((fscanf(fp,"%d:%d,%d\n",&id,&arrival,&service))) {
      // printf("%d:%d,%d\n",id,arrival,service);
      //if(!(fscanf(fp,"%d:%d,%d\n",&id,&arrival,&service))){
  	  //   printf("Error: input not in format of 'id:arrival time,service time'\n");
      //   exit(1);
  	  // }
      if (id<0 || arrival<0 || service<0) {
        printf("Error: negative input is filtered out\n");
        NCustomers--;
        //printf("%d:%d,%d\n",id,arrival,service);
      } else {
        customer_list[curr_customer].user_id=id;
    	  customer_list[curr_customer].arrival_time=arrival;
    	  customer_list[curr_customer].service_time=service;
        curr_customer++;
      }
    }
  }
  fclose(fp);

  int i;
  printf("# of customers: %d\n",NCustomers);
  for(i = 0; i < NCustomers; i++) {
    printf("%d:%d,%d\n", customer_list[i].user_id, customer_list[i].arrival_time, customer_list[i].service_time);
  }

  if (NCustomers <= 0){
		printf("Error: Number of customers should be a positive number\n");
		return -1;
	}

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

	//create clerk and customer threads
	for(i = 0; i < NCLERK; i++){
		pthread_create(&clerk_threads[i], NULL, clerk_entry, (void *)&clerk_info[i]);
	}

	for(i = 0; i < NCustomers; i++) {
		pthread_create(&customer_threads[i], NULL, customer_entry, (void *)&customer_list[i]);
	}

	// wait for all customer threads to terminate
	for(i = 0; i < NCustomers; i++) {
		pthread_join(customer_threads[i], NULL);
	}

	// destroy mutex & condition variable
	for (i = 0; i < NQUEUE; i++) {
		pthread_mutex_destroy(&mutex_queue[i]);
		pthread_cond_destroy(&convar_queue[i]);
	}

	for (i = 0; i < NCLERK; i++) {
		pthread_mutex_destroy(&mutex_clerk[i]);
		pthread_cond_destroy(&convar_clerk[i]);
	}

	//calculate the average waiting time of all customers
  overall_waiting_time = 0.0;
  for(i = 0; i < NCustomers; i++) { // number of customers
	   overall_waiting_time += customer_list[i].waiting_time_total;
	}
  double avg_waiting_time = overall_waiting_time/NCustomers;
  fprintf(stdout, "The average waiting time for all customers in the system is %.2f seconds. \n", avg_waiting_time);
	return 0;
}
