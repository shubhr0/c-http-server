#include<stdio.h>
#include<poll.h>

int main(void)
{
	struct pollfd pfds[1];  //more if you want to monitor more 
	pfds[0].fd=0;  //standard input
	pfds[0].events=POLLIN;   //tell me when ready to read
	


	//if you needed to monitor other things,as well:
	//pfds[1].fd=some_socket;  //some socket descriptor
	//pfds[1].events = POLLIN; //tell me when ready to read
	

	printf("hit RETURN or wait for 2.5 seconds for timeout\n");

	int num_events = poll(pfds,1,2500);  //2.5 second timeout
	

	if(num_events==0){
		printf("poll timed out!\n");
	}else{
		int pollin_happened = pfds[0].revents & POLLIN;

		if(pollin_happened){
			printf("file descriptor %d is ready to read\n",pfds[0].fd);

		}else{
			printf("unexpected event occured: %d\n",pfds[0].revents);
		}
	}

	return 0;
}
