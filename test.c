#include <stdio.h>
#include <stdlib.h>

int main() {
  int minNum = 6;
  // generate random numbers between 0 and 6
  int i;
  for (i = 0; i<100; i++) {
    pick_random_queue(arr,minNum);
  }
}

int pick_random_queue(int array[NQUEUE],int minNum) {
	int result;
	int index = rand() % (minNum+1);
	printf("random index: %d\n",index);
	result = array[index];
	printf("value: %d\n", result);
	return result;
}
