#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<poll.h>

#define PORT "9034"  //port we're listening on


/*
 * convert socket to IP address string.
 * addr:struct sockaddr_in or struct sockaddr_in6
 */

const char *inet_ntop2(void *addr,char *buf,size_t size)
{
	struct sockaddr_storage *sas = addr;
	struct sockaddr_in *sa4;
	struct sockaddr_in6 *sa6;
	void *src;



	switch(sas->ss_family){
		case AF_INET:
			sa4=addr;
			src=&(sa4->sin_addr);
			break;
		case AF_INET6:
			sa6 = addr;
			src=&(sa6->sin6_addr);
			break;
		default:
			return NULL;
	}


	return inet_ntop(sas->family,src,buf,size);
}

/*
 * return a listening socket.
 */

int get_listener_socket(void)
{
	int listener;    //listening socket descriptor
	int yes=1;       //for setsocketopt() SO_REUSEADDR,below
	int rv;



	struct addrinfo hints,*ai,*p;

	//get us a socket and bind it
	memset(&hints,0,sizeof hints);
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags=AI_PASSIVE;
	if((rv=getaddrinfo(NULL,PORT,&hints,&ai))!=0){
		fprintf(stderr,"pollserver: %s\n",gai_strerror(rv));
		exit(1);
	}

	for(p=ai;p!=NULL;p=p->ai_next){
		listener=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
		if(listener<0){
			continue;
		}

		//lose the pesky "address already in use"error message

		setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));
		if(bind(listener,p->ai_addr,p->ai_addrlen)<0){
			close(listener);
			continue;
		}


		break;
	}


	//if we not here,it means we didnt get bound
	if(p==NULL){
		return -1;
	}

	freeaddrinfo(ai);  //all done with this
	

	//listen
	
	if(listen(listener,10) == -1){
		return -1;
	}

	return listener;
}


/*
 * add a new file descriptor to the set
 */

void add_to_pfds(struct pollfd **pfds,int newfd,int *fd_count,int *fd_size)
{
	//if we dont have room,add more space in the pfds array
	if(*fd_count == *fd_size){
		*fd_size *= 2;  //double it
		*pfds = realloc(*pfds,sizeof(**pfds) * (*fd_size));
	}


	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN;    //check ready to end
	(*pfds)[*fd_count].revents = 0;


	(*fd_count)++;
}


/*
 * remove a file descriptor at a given index from the set
 */

void del_from_pfds(struct pollfd pfds[],int i,int *fd_count)
{
	//copy the one from the end over this one
	pfds[i]=pfds[*fd_count-1];

	(*fd_count)--;
}

/*
 * handle incoming connections
 */
void handle_new_connection(int listener,int *fd_count,int *fd_size,struct pollfd **pfds)
{
	struct sockaddr_storage remoteaddr;  //client address
	socklen_t addrlen;
	int newfd;   //newly accept()ed socket descriptor
	char remoteIP[INET6_ADDRSTRLEN];
	addrlen = sizeof remoteaddr;
	newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);

	if(newfd == -1){
		perror("accept");
	}else{
		add_to_pfds(pfds,newfd,fd_count,fd_size);

		printf("pollserver:new connection from %s on socket %d\n",
			inet_ntop2(&remoteaddr,remoteIP,sizeof remoteIP),
			newfd);
	}
}

/*
 * handle regular client data or client hangups
 */

void handle_client_data(int listener,int *fd_count,struct pollfd *pfds,int *pfd_i)
{
	char buf[256];   //buffer for client data
	

	int nbytes=recv(pfds[*pfd_i].fd,buf,sizeof buf,0);

	int sender_fd=pfds[*pfd_i].fd;

	if(nbytes<=0){  //got error or connectikn closed by client
		if(nbytes == 0){
			//connection closed
			prints("pollserver:socket %d hung up\n",sender_fd);
		}else{
			perror("recv");
		}

		close(pfds[*pfd_i].fd);  //bye!

		del_from_pfds(pfds,*pfd_i,fd_count);

		//reexamine the slot we just deleted
		(*pfd_i)--;
	}else{ //we got some good data from client
	       printf("pollserver:recv from fd %d:%.*s",sender_fd,nbytes,buf);

	       //send to everyone!
	       for(int j=0;j<*fd_count;j++){
		       int dest_fd=pfds[j].fd;

		       //except the listener and oureselves
		       if(dest_fd != listener && dest_fd != sender_fd){
			       if(send(dest_fd,buf,nbytes,0) == -1){
				       perror("send");
				      }
		       }
		     }
	}
}


/*
 * process all existing connections.
 */
void process_connections(int listener,int *fd_count,int *fd_size,struct pollfd **pfds)
{
	for(int i=0;i<*fd_count;i++){
		//check if someone's ready to read
		if((*pfds)[i].revents & (POLLIN|POLLHUP)){
			//we got one!
			
			if((*pfds)[i].fd ==listener){
				//if we're the listener,its a new connection
				handle_new_connection(listener,fd_count,fd_size,pfds);
			}else{
				//otherwise just a regular client
				handle_client_data(listener,fd_count,*pfds,&i);
			}
		}
	}
}

/*
 * main:create a listener and connection set,loop forever
 * processing connection.
 */

int main(void)
{
	int listener;   //listening socket descriptor
	
	//start off with room for 5 connections
	//(we'll realloc as necessary)
	int fd_size=5;
	int fd_count =0;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	//set up and get a listening socket
	listener = get_listener_socket();

	if(listener ==-1){
		fprintf(stderr,"error getting listening socket\n");
		exit(1);
	}

	//add the listener to set;
	//report ready to read on incoming connection
	

	pfds[0].fd = listener;
	pfds[0].events= POLLIN;

	fd_count =1;   //for the listener
	

	puts("pollserver:waiting for connections...");

	//main loop
	
	for(;;){
		int poll_count=poll(pfds,fd_count,-1);

		if(poll_count == -1){
			perror("poll");
			exit(1);
		}

		//run through connections looking for data to read
		process_connections(listener,&fd_count,&fd_size,&pfds);
	}


	free(pfds);

}

















