#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include<string.h>

/*
int getaddrinfo(const char *node,
		const char *service,
		const struct addrinfo *hints,
		struct addrinfo **res);


int status;
int addrinfo hints;
int addrinfo *servinfo;

memset(&hints,0,sizeof hints);
hints.ai_family=AF_UNSPEC;
hints.ai_socktype=SOCK_STREAM;
hints.ai_flags=AI_PASSIVE;


if((status=getaddrinfo(NULL,"3490",&hints,&servinfo)) !=0){
	fprintf(stderr,"gai error: %s\n",gai_stderror(status));
	exit(1);
}

//servinfo now points to a linkedlist of 1 or more
//struct addrinfos


//do everything until you dont need servinfo anymore....


freeaddrinfo(servinfo);  //free the linkedlist


*/
int main(void){
	struct sockaddr_storage their_addr;
	socklen_t addr_size;
	struct addrinfo hints,*res;
	int sockfd,new_fd;



	memset(&hints,0,sizeof hints);
	hints.ai_famly=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags=AI_PASSIVE;
	
	status=getaddrinfo("NULL","3490",&hints,&res);

	if(status !=0){
		fprintf(stderr,"gai error: %s\n",gai_strerror(status));
		exit(1);
	}
        

	sockfd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if(sockfd==-1){
		perror("socket");
		exit(1);
	}

	if(bind(sockfd,res->ai_addr,res->ai_addrlen)==-1){
		perror("bind");
		close(sockfd);
		exit(1);
	}

	freeaddrinfo(res);

	if(listen(sockfd,BACKLOG)==-1){
		perror("listen");
		close(sockfd);
		exit(1);
	}



	printf("server waiting for connection...");

	addr_size=sizeof their_addr;


	new_fd=accept(sockfd,(struct sockaddr *)&their_addr,&addr_size);

	if(new_fd==-1){
		perror("accept");
		exit(1);
	}


	printf("connection accepted");


	close(new_fd);
	close(sockfd);


	return 0;
}

	









