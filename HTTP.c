#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/stat.h>
#include<limits.h>
#include<errno.h>
#include<pthread.h>
#include<sys/time.h>

#define TIMEOUT_SECONDS 5
#define PORT 8080
#define BUFFER_SIZE 4096
#define QUEUE_SIZE 1000
#define NUM_THREADS 10

typedef struct{
	int *items;
	int capacity;
	int head;
	int tail;
	int count;
	pthread_mutex_t mutex;
	pthread_cond_t cond_not_empty;
	pthread_cond_t cond_not_full;
}queue_t;


queue_t queue;
pthread_t workers[NUM_THREADS];

void queue_init(queue_t *q,int capacity){
	q->items= malloc(capacity * sizeof(int));
	q->capacity = capacity;
	q->head=0;
	q->tail=0;
	q->count=0;
	pthread_mutex_init(&q->mutex,NULL);
	pthread_cond_init(&q->cond_not_empty,NULL);
	pthread_cond_init(&q->cond_not_full,NULL);
}

void enqueue(int value)
{
	pthread_mutex_lock(&queue.mutex);

	while(queue.count == queue.capacity){
		pthread_cond_wait(&queue.cond_not_full,&queue.mutex);
	}

	queue.items[queue.tail]=value;
	queue.tail=(queue.tail + 1) % queue.capacity;
	queue.count++;

	pthread_cond_signal(&queue.cond_not_empty);
	pthread_mutex_unlock(&queue.mutex);
}

int dequeue()
{
	pthread_mutex_lock(&queue.mutex);

	while(queue.count == 0){
		pthread_cond_wait(&queue.cond_not_empty,&queue.mutex);
	}

	int value = queue.items[queue.head];
	queue.head = (queue.head + 1) % queue.capacity;
	queue.count--;

	pthread_cond_signal(&queue.cond_not_full);
	pthread_mutex_unlock(&queue.mutex);
	return value;
}
void handle_connection(int client_fd);

void *worker_thread(void *arg)
{
	int id=*(int *)arg;
	printf("Worker %d started\n",id);

	while(1){
		int client_fd = dequeue();
		printf("worker %d processing %d\n",id,client_fd);
		handle_connection(client_fd);
	}

	return NULL;
}
			


static const char *mime_type(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html";
    if (strcmp(dot, ".css")  == 0) return "text/css";
    if (strcmp(dot, ".js")   == 0) return "application/javascript";
    if (strcmp(dot, ".png")  == 0) return "image/png";
    if (strcmp(dot, ".jpg")  == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".gif")  == 0) return "image/gif";
    if (strcmp(dot, ".ico")  == 0) return "image/x-icon";
    return "text/plain";
}

int should_keep_alive(char *headers)
{
	char *conn = strstr(headers,"Connection:");
	if(conn == NULL){
		return 1;
	}

	conn+=strlen("Connection:");
	while(*conn == ' ') conn++;

	if(strncasecmp(conn,"close",5) == 0){
		return 0;
	}

	return 1;
}



void handle_connection(int client_fd)
{
	struct timeval timeout;
        timeout.tv_sec = TIMEOUT_SECONDS;
        timeout.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	while(1){
		
		char buffer[BUFFER_SIZE];
		int total_received=0;

		//read headers

		while(total_received<BUFFER_SIZE-1){
			int bytes=recv(client_fd,buffer+total_received,BUFFER_SIZE-total_received-1,0);
			if(bytes<=0) break;

			total_received+=bytes;
			buffer[total_received]='\0';

			if(strstr(buffer,"\r\n\r\n")!=NULL) break;
		}

		int keep_alive = should_keep_alive(buffer);

		char *line_end=strstr(buffer,"\r\n\r\n");
		if(line_end==NULL){
			printf("invalid request");
			return;
		}
	
		//parse request line

		int line_length=line_end-buffer;
		char request_line[256];
		strncpy(request_line,buffer,line_length);
		request_line[line_length]='\0';
		char method[16],path[256],version[16];
		sscanf(request_line,"%15s %255s %15s",method,path,version);

		//handle POST request
	
		if(strstr(method,"POST")){
			//find Content-Length
			char *content_length_ptr=strstr(buffer,"Content-Length:");
			if(content_length_ptr ==NULL){
				printf("POST without content length");
				close(client_fd);
				return;
			}

			//parse content-length value

			content_length_ptr += strlen("Content-Length:");
			while(*content_length_ptr ==' '){
				content_length_ptr++;
			}
			int content_length=atoi(content_length_ptr);
	
			printf("POST request,Content-Length: %d\n",content_length);
	
			//calculate body position
		
			char *body_start=line_end + 4;
			int body_bytes_in_buffer=total_received-(body_start - buffer);
	
			//allocate and read body		
			char body[BUFFER_SIZE];
			memset(body,0,BUFFER_SIZE);

			//copy what we already have
	
			if(body_bytes_in_buffer>0){
				memcpy(body,body_start,body_bytes_in_buffer);
			}
	
			//read remaining body bytes
	
			int bytes_read = body_bytes_in_buffer;
			while(bytes_read < content_length){
				int n=recv(client_fd,body+bytes_read,content_length-bytes_read,0);
				if(n<=0) break;
				bytes_read +=n;
				}
			
			body[bytes_read] = '\0';  //null terminate
	
			printf("Received body: %s\n", body);
		
			
		        // Send response echoing the body
        		char response[BUFFER_SIZE];
       		        int response_len = snprintf(response, sizeof(response),
        	     			   "HTTP/1.1 200 OK\r\n"
        	   			   "Content-Length: %d\r\n"
        	   			   "Content-Type: text/plain\r\n"
        	   			   "Connection: %s\r\n"
        	  			   "\r\n"
        	  			   "You sent: %s",
        	    			    bytes_read + 10,  // "You sent: " is 10 chars
					    keep_alive ? "keep-alive" : "close",
        	   			    body
       			 );
	
			send(client_fd, response, response_len, 0);
			if(!keep_alive){
				close(client_fd);
				return;
			}	 
		}


		char filepath[300];
		const char *body1="403 forbidden";
		char response_path[256];
		int len = snprintf(response_path,sizeof(response_path),
			"HTTP/1.1 403 Forbidden\r\n"
   			 "Content-Length: %zu\r\n"
   			 "Content-Type: text/plain\r\n"
   			 "Connection: close\r\n"
   			 "\r\n"
   			 "%s",
   			 strlen(body1), body1
		);
		if(strstr(path,"..")){
			send(client_fd,response_path,len,0);
			close(client_fd);
			return;
		}	
		int cx=snprintf(filepath,300,"./public%s",path);
		char public_root[PATH_MAX];
		if(realpath("./public",public_root)==NULL){
			perror("realpath public");
			return;
		}
		char resolved_path[PATH_MAX];
		if(realpath(filepath,resolved_path)==NULL){
			perror("error1");
			return;
		}
		if(strncmp(resolved_path,public_root,strlen(public_root))!= 0){
			perror("error2");
			return;
		}
		FILE *fptr;
		fptr=fopen(resolved_path,"rb");
		if(fptr==NULL){
			const char *body2="404 not found";
        		char response_file[256];
        		int len=snprintf(response_file,sizeof(response_file),
        	       		 "HTTP/1.1 404 Not Found\r\n"
                		 "Content-Length: %zu\r\n"
                		 "Content-Type: text/plain\r\n"
                		 "Connection: close\r\n"
                		 "\r\n"
                		 "%s",
                		 strlen(body2), body2
       				 );
				send(client_fd,response_file,len,0);
				close(client_fd);
				return;
			}

		off_t file_size;
		struct stat st;
		if(stat(resolved_path,&st)==0){
			file_size=st.st_size;
		}else{
			perror("error getting file size");
			return;
		}
		const char *content_type = mime_type(resolved_path);
		char response_headers[256];
		int len1=snprintf(response_headers,sizeof(response_headers),
			"HTTP/1.1 200 OK\r\n"
			"Content-Length: %zu\r\n"
			"Content-Type: %s\r\n"
			"Connection: %s\r\n"
			"\r\n",
			(size_t)file_size,
			content_type,
			keep_alive ? "keep-alive" : "close"
			);
		ssize_t total_sent=0;
		while(total_sent<len1){
			ssize_t n=send(client_fd,response_headers+total_sent,len1-total_sent,0);
			if(n<=0){
				perror("send headers failed");
				close(client_fd);
				return;
			}
			total_sent+=n;
		}

		size_t bytes_read;
	
		while((bytes_read=fread(buffer,1,BUFFER_SIZE,fptr))>0){
			size_t sent=0;
			while(sent< bytes_read){
	
				ssize_t n=send(client_fd,buffer+sent,bytes_read - sent,0);
				if(n<=0){
					perror("send file failed");
					fclose(fptr);
					close(client_fd);
					return;
				}
				
				sent+=n;
			}
		}	

		fclose(fptr);
		if(!keep_alive){	
			close(client_fd);
			return;
		}
	}
}
int main(void)
{

	queue_init(&queue,QUEUE_SIZE);

	int thread_ids[NUM_THREADS];
	for(int i=0;i<NUM_THREADS;i++){
		thread_ids[i]=i;
	}
	for(int i=0;i<NUM_THREADS;i++){
		pthread_create(&workers[i],NULL,worker_thread,&thread_ids[i]);
	}
	int server_fd=socket(AF_INET,SOCK_STREAM,0);
	if(server_fd<0){ perror("socket");return 1;}
	
	int opt=1;
	setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	struct sockaddr_in addr ={
		.sin_family=AF_INET,
		.sin_port=htons(PORT),
		.sin_addr.s_addr=INADDR_ANY,
	};

	if(bind(server_fd,(struct sockaddr *)&addr,sizeof(addr))<0){
		perror("bind"); return 1;
	}

	if(listen(server_fd,10)<0){ perror("listen"); return 1; }

	printf("serving ./public on http://localhost:%d\n",PORT);
	
	while(1){
		int client_fd=accept(server_fd,NULL,NULL);
		if(client_fd<0) {perror("acceot"); continue;}
		enqueue(client_fd);

	}

	
	close(server_fd);
	return 0;

}



