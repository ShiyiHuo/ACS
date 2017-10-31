#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>


typedef struct customer{ /// use this struct to record the customer information read from customers.txt
	int user_id;
	int service_time;
	int arrival_time;
	double waiting_time_start;
	double waiting_time_end;
	double waiting_time_total;
} customer;

/* global variables */
#define TRUE 1
#define FALSE 0
#define MAX_CUSTOMER 256
#define MAX_INPUT_SIZE 1024
#define NQUEUE 4
#define NCLERK 2
#define TO_UTIME 1000000

customer customer_list[MAX_CUSTOMER];  //list of customers
int clerk_info[NCLERK] = {0,1};  //list of clerks (clerkid)

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

int findShortestQueue(){
	int min_queue_length = MAX_CUSTOMER+1;
	int qnum;
	pthread_mutex_lock(&mutex_queue_length);
	{
		int i;
		for (i = 0; i < NQUEUE; i++) {
			//printf("CUSTOMER - queue length of queue %d is %d\n",i, queue_length[i]);
			if (queue_length[i] < min_queue_length) {
				min_queue_length = queue_length[i];
				qnum = i;
			}
		}
		queue_length[qnum]++;
	}
  	pthread_mutex_unlock(&mutex_queue_length);

  	return qnum;
}

// function entry for customer threads
void * customer_entry(void * cus_info){
	struct customer * p_myInfo = (struct customer *) cus_info;
	usleep((p_myInfo->arrival_time)*TO_UTIME/10); //sleep as long as arrival time
	fprintf(stdout, "A customer arrives: customer ID %2d. \n", p_myInfo->user_id);

	int shortest_queue = findShortestQueue();

	pthread_mutex_lock(&mutex_queue[shortest_queue]);
	{
		fprintf(stdout, "A customer enters a queue: the queue ID %1d, and length of the queue %2d. \n", shortest_queue, queue_length[shortest_queue]);
		p_myInfo->waiting_time_start = getTimeDifference();
		// customers are waiting for a clerk
		pthread_cond_wait(&convar_queue[shortest_queue], &mutex_queue[shortest_queue]);
		//Now pthread_cond_wait returned, customer was awaked by one of the clerks
	}
	pthread_mutex_unlock(&mutex_queue[shortest_queue]);
	
	p_myInfo->waiting_time_end = getTimeDifference();
	p_myInfo->waiting_time_total = p_myInfo->waiting_time_end - p_myInfo->waiting_time_start;


  /* Try to figure out which clerk awaked me, because you need to print the clerk Id information */
	int clerk;
	pthread_mutex_lock(&mutex_C);
	{
		if (C == 0) { //clerk 0 awakes me
			clerk = 0;
			C = -1;
		} else if (C == 1) {  //clerk 1 awakes me
			clerk = 1;
			C = -1;
		}
	}
	pthread_mutex_unlock(&mutex_C);

	/* get the current machine time; updates the overall_waiting_time*/
	fprintf(stdout, "A clerk starts serving a customer: start time %.2f, the customer ID %2d, the clerk ID %1d. \n", getTimeDifference(), p_myInfo->user_id, clerk);

	usleep((p_myInfo->service_time)*TO_UTIME/10);

	/* get the current machine time; */
	fprintf(stdout, "A clerk finishes serving a customer: end time %.2f, the customer ID %2d, the clerk ID %1d. \n", getTimeDifference(), p_myInfo->user_id, clerk);
	pthread_mutex_lock(&mutex_clerk[clerk]);
	{
		pthread_cond_signal(&convar_clerk[clerk]); //Notice the clerk that service is finished, it can serve another customer
	}
	pthread_mutex_unlock(&mutex_clerk[clerk]);

	pthread_exit(NULL);

	return NULL;
}

int findLongestQueue(int clerkId){

	int i, qnum = 0, max_queue_length = 0;

	pthread_mutex_lock(&mutex_C);
	{
		if(C != -1){
			pthread_mutex_unlock(&mutex_C);
			return -1;
		}
		pthread_mutex_lock(&mutex_queue_length);
		{
			for(i = 0; i < NQUEUE; i++){
				if(i == 0){
					max_queue_length = queue_length[0];
				}
				else{
					if(max_queue_length < queue_length[i]){
						max_queue_length = queue_length[i];
						qnum = i;
					}
				}
			}
			if(max_queue_length > 0){
				queue_length[qnum]--;
				C = clerkId;
			}
			else{
				qnum = -1;
			}
		}
		pthread_mutex_unlock(&mutex_queue_length);
	}
	pthread_mutex_unlock(&mutex_C);

	return qnum;
}

void *clerk_entry(void * clerkNum){

	int longest_queue;
	int myClerkId = *(int *)clerkNum;

	while(TRUE){

		if((longest_queue = findLongestQueue(myClerkId)) == -1){
			usleep(100);
			continue;
		}

		pthread_mutex_lock(&mutex_queue[longest_queue]);
		{
			pthread_cond_signal(&convar_queue[longest_queue]);
		}
		pthread_mutex_unlock(&mutex_queue[longest_queue]);


		pthread_mutex_lock(&mutex_clerk[myClerkId]);
		{
			pthread_cond_wait(&convar_clerk[myClerkId], &mutex_clerk[myClerkId]);
		}
		pthread_mutex_unlock(&mutex_clerk[myClerkId]);
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
		printf("customer %d's waiting time: %f\n",customer_list[i].user_id, customer_list[i].waiting_time_total);
	}
	double avg_waiting_time = overall_waiting_time/NCustomers;
	fprintf(stdout, "The average waiting time for all customers in the system is %.2f seconds. \n", avg_waiting_time);

	return 0;
}
