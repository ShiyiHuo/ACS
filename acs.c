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

int pick_random_queue(int array[NQUEUE],int minNum) {
	int result;
	// printf("pick random number from 0 to %d\n", minNum-1);
	int index = rand() % (minNum);
	// printf("random index: %d\n",index);
	result = array[index];
	// printf("value: %d\n", result);
	return result;
}

int findShortestQueue(){
	int min_queue_length = MAX_CUSTOMER+1;
	int min[NQUEUE] = {-1,-1,-1,-1};	// index of minimum queues
	int min_num = 0;	// number of minimums
	int result;
	pthread_mutex_lock(&mutex_queue_length);
	{
		int i;
		//find min queue length
		for (i = 0; i < NQUEUE; i++) {
			if (queue_length[i] < min_queue_length) {
				min_queue_length = queue_length[i];
			}
		}
		// printf("min queue length: %d\n", min_queue_length);

		//construct min array
		for (i = 0; i < NQUEUE; i++) {
			if(queue_length[i] == min_queue_length) {
				min[min_num] = i;
				min_num++;
			}
		}

		// for (i = 0; i < NQUEUE; i++) {
		// 	printf("min array %d: %d\n", i, min[i]);
		// }

		// pick random shortest queue
		result = pick_random_queue(min,min_num);

		// printf("pick random number from 0 to %d\n", min_num-1);
		// int index = rand() % (min_num);
		// printf("random index: %d\n",index);
		// result = min[index];
		// printf("value: %d\n", result);
		queue_length[result]++;
	}
	pthread_mutex_unlock(&mutex_queue_length);

	return result;
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

	int i;
	// int qnum = 0;
	int max_queue_length = 0;
	int max[NQUEUE] = {-1,-1,-1,-1};	// index of maximum queues
	int max_num = 0;	// number of maximums
	int result;

	pthread_mutex_lock(&mutex_C);
	{
		if(C != -1){
			pthread_mutex_unlock(&mutex_C);
			return -1;
		}
		pthread_mutex_lock(&mutex_queue_length);
		{
			// for(i = 0; i < NQUEUE; i++){
			// 	if(i == 0){
			// 		max_queue_length = queue_length[0];
			// 	}
			// 	else{
			// 		if(max_queue_length < queue_length[i]){
			// 			max_queue_length = queue_length[i];
			// 			qnum = i;
			// 		}
			// 	}
			// }
			// if(max_queue_length > 0){
			// 	queue_length[qnum]--;
			// 	C = clerkId;
			// }
			// else{
			// 	qnum = -1;
			// }


			//find max queue length
			for (i = 0; i < NQUEUE; i++) {
				if(i == 0){
					max_queue_length = queue_length[0];
				}
				else{
					if(max_queue_length < queue_length[i]){
						max_queue_length = queue_length[i];
					}
				}
			}
			//printf("max queue length: %d\n", max_queue_length);

			if(max_queue_length > 0){
				//construct max array
				for (i = 0; i < NQUEUE; i++) {
					if(queue_length[i] == max_queue_length) {
						max[max_num] = i;
						max_num++;
					}
				}

				// for (i = 0; i < NQUEUE; i++) {
				// 	printf("max array %d: %d\n", i, max[i]);
				// }

				// pick random shortest queue
				result = pick_random_queue(max,max_num);

				// printf("pick random number from 0 to %d\n", max_num-1);
				// int index = rand() % (max_num);
				// printf("random index: %d\n",index);
				// result = max[index];
				// printf("value: %d\n", result);
				queue_length[result]--;
				C = clerkId;
			}
			else{
				result = -1;
			}


		}
		pthread_mutex_unlock(&mutex_queue_length);
	}
	pthread_mutex_unlock(&mutex_C);

	return result;
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

	//initialize all mutexes and condition variables
	for (i = 0; i < 4; i++) {
		if(pthread_mutex_init(&mutex_queue[i], NULL) != 0) {
      printf("Error: Failed to initialize mutex for queue\n");
      exit(1);
    }
    if(pthread_cond_init(&convar_queue[i], NULL) != 0) {
      printf("Error: Failed to initialize conditional variable for queue\n");
      exit(1);
    }
	}

	for (i = 0; i < 2; i++) {
    if(pthread_mutex_init(&mutex_clerk[i], NULL) != 0) {
      printf("Error: Failed to initialize mutex for clerk\n");
      exit(1);
    }
    if(pthread_cond_init(&convar_clerk[i], NULL) != 0) {
      printf("Error: Failed to initialize conditional variable for clerk\n");
      exit(1);
    }
	}

  if(pthread_mutex_init(&mutex_queue_length, NULL) != 0) {
    printf("Error: Failed to initialize mutex for queue length\n");
    exit(1);
  }
  if(pthread_mutex_init(&mutex_C, NULL) != 0) {
    printf("Error: Failed to initialize mutex for C\n");
    exit(1);
  }

	//get global init_time
	gettimeofday(&init_time,NULL);

	//create clerk and customer threads
	for(i = 0; i < NCLERK; i++){
		if(pthread_create(&clerk_threads[i], NULL, clerk_entry, (void *)&clerk_info[i]) != 0) {
      printf("Error: Failed to create clerk thread\n");
      exit(1);
    }
	}

	for(i = 0; i < NCustomers; i++) {
    if(pthread_create(&customer_threads[i], NULL, customer_entry, (void *)&customer_list[i]) != 0) {
      printf("Error: Failed to create customer thread\n");
      exit(1);
    }
	}

	// wait for all customer threads to terminate
	for(i = 0; i < NCustomers; i++) {
    if(pthread_join(customer_threads[i], NULL) != 0) {
      printf("Error: Failed to join customer thread\n");
      exit(1);
    }
	}

	// destroy mutex & condition variable
	for (i = 0; i < NQUEUE; i++) {
    if(pthread_mutex_destroy(&mutex_queue[i]) != 0) {
      printf("Error: Failed to destroy mutex for queue\n");
      exit(1);
    }
    if(pthread_cond_destroy(&convar_queue[i]) != 0) {
      printf("Error: Failed to destroy conditional variable for queue\n");
      exit(1);
    }
	}

	for (i = 0; i < NCLERK; i++) {
    if(pthread_mutex_destroy(&mutex_clerk[i]) != 0) {
      printf("Error: Failed to destroy mutex for clerk\n");
      exit(1);
    }
    if(pthread_cond_destroy(&convar_clerk[i]) != 0) {
      printf("Error: Failed to destroy conditional variable for clerk\n");
      exit(1);
    }
	}

  if(pthread_mutex_destroy(&mutex_queue_length) != 0) {
    printf("Error: Failed to destroy mutex for queue length\n");
    exit(1);
  }
  if(pthread_mutex_destroy(&mutex_C) != 0) {
    printf("Error: Failed to destroy mutex for C\n");
    exit(1);
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
