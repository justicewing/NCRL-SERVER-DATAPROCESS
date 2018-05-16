#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include <math.h>
#include <time.h>
#include <complex.h>
#include "mkl.h"
#include "thread_pool.h"
#include "srslte/fec/cbsegm.h"
#include "TaskScheduler.h"

const int threadNum_tx = 1;
const int threadNum_rx = 1;

pthread_mutex_t mutex1_tx;
pthread_mutex_t mutex2_tx;

pthread_mutex_t mutex1_rx;
pthread_mutex_t mutex2_rx;
pthread_mutex_t mutex3_rx;

sem_t exit_signal;

//test
sem_t tx_signal;
sem_t rx_signal;
int main(){
	
	pthread_mutex_init(&mutex1_tx,NULL);
	pthread_mutex_init(&mutex2_tx,NULL);
	pthread_mutex_init(&mutex1_rx,NULL);
	pthread_mutex_init(&mutex2_rx,NULL);
	pthread_mutex_init(&mutex3_rx,NULL);
	
	sem_init(&exit_signal,0,0);
	sem_init(&tx_signal,0,0);
	sem_init(&rx_signal,0,0);
	
	pool_init(0, 1, 0);printf("creat pool 0...\n");
	pool_init(1, 1, 1);printf("creat pool 1...\n");
	pool_init(2, threadNum_tx, 2);printf("creat pool 2...\n");
	pool_init(2 + threadNum_tx, threadNum_rx, 3);printf("creat pool 3...\n");
	
	pool_add_task(TaskScheduler_tx, NULL, 0);printf("add tx taskScheduler to pool 0...\n");
	pool_add_task(TaskScheduler_rx, NULL, 1);printf("add rx taskScheduler to pool 1...\n");

	for(int i = 0; i < 2; i++) sem_wait(&exit_signal);

	pool_destroy(0);
	pool_destroy(1);

	sem_destroy(&exit_signal);

	return 0;
}
