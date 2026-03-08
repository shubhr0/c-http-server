#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

#define NUM_THREADS 4
#define QUEUE_SIZE 100

typedef struct{
	void (*function)(void *);
	void *arg;
}task_t;

task_t task_queue[QUEUE_SIZE];
int queue_head=0;
int queue_tail=0;
int queue_count=0;
int shutdown = 0;

pthread_mutex_t mutex;
pthread_cond_t cond;

void thread_pool_submit(void(*func)(void *),void *arg){
	pthread_mutex_lock(&mutex);

	if(queue_count ==QUEUE_SIZE){
		printf("Queue full!\n");
		pthread_mutex_unlock(&mutex);
		return;
	}

	task_queue[queue_tail].function = func;
	task_queue[queue_tail].arg = arg;
	queue_tail = (queue_tail + 1) % QUEUE_SIZE;
	queue_count++;

	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
}

void *worker_thread(void *arg){
	int id =*(int *)arg;
	printf("worker %d started\n",id);

	while(1){
		pthread_mutex_lock(&mutex);

		while(queue_count == 0 && !shutdown){
			pthread_cond_wait(&cond,&mutex);
		}

		if(shutdown && queue_count == 0){
			pthread_mutex_unlock(&mutex);
			break;
		}

		task_t task = task_queue[queue_head];
		queue_head = (queue_head + 1) % QUEUE_SIZE;
		queue_count--;

		pthread_mutex_unlock(&mutex);

		task.function(task.arg);
	}

	printf("workers %d exiting\n",id);
	return NULL;
}

void print_task(void *arg){
	int num = *(int *)arg;
	printf("task executing: %d\n",num);
	sleep(1);
	free(arg);
}
	

int main()
{
	pthread_mutex_init(&mutex,NULL);
	pthread_cond_init(&cond,NULL);

	pthread_t threads[NUM_THREADS];
	int threads_ids[NUM_THREADS];

	for(int i=0;i<NUM_THREADS;i++){
		threads_ids[i] = i;
		pthread_create(&threads[i],NULL,worker_thread,&threads_ids[i]);
	}

	for(int i =0; i< 20;i++){
		int *arg = malloc(sizeof(int));
		*arg = i;
		thread_pool_submit(print_task,arg);
	}

	sleep(10);

	pthread_mutex_lock(&mutex);
	shutdown =1;
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);

	for(int i=0;i< NUM_THREADS;i++){
		pthread_join(threads[i],NULL);
	}

	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond);
	return 0;
}
