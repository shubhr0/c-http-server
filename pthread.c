#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void *worker(void *arg) {
    int id=*(int *)arg;
    free(arg);
    printf("worker:%d starting\n",id);
    sleep(2);
    printf("worker %d done\n",id);
    return NULL;
}

int main() {
	printf("creating 5 workers...\n");

	for(int i=0;i<5;i++){
		int *id=malloc(sizeof(int));
		*id =i;
        	pthread_t thread;
    		pthread_create(&thread, NULL, worker, id);
    		pthread_detach(thread);
	}
    
    
    	sleep(3);	// Wait for thread to finish
	printf("all workers completed\n");
    	return 0;

}
