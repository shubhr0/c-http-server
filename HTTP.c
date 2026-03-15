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
#include<arpa/inet.h>
#include<time.h>

#define TIMEOUT_SECONDS 5
#define PORT 8080
#define BUFFER_SIZE 4096
#define QUEUE_SIZE 1000
#define NUM_THREADS 10

typedef struct{
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
}connection_t;

typedef struct{
    connection_t *items;
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
    q->items=malloc(sizeof(connection_t)*capacity);
    q->capacity=capacity;
    q->head=0;
    q->tail=0;
    q->count=0;
    pthread_mutex_init(&q->mutex,NULL);
    pthread_cond_init(&q->cond_not_empty,NULL);
    pthread_cond_init(&q->cond_not_full,NULL);
}

void enqueue(connection_t value){
    pthread_mutex_lock(&queue.mutex);

    while(queue.count==queue.capacity){
        pthread_cond_wait(&queue.cond_not_full,&queue.mutex);
    }

    queue.items[queue.tail]=value;
    queue.tail=(queue.tail+1)%queue.capacity;
    queue.count++;

    pthread_cond_signal(&queue.cond_not_empty);
    pthread_mutex_unlock(&queue.mutex);
}

connection_t dequeue(){
    pthread_mutex_lock(&queue.mutex);

    while(queue.count==0){
        pthread_cond_wait(&queue.cond_not_empty,&queue.mutex);
    }

    connection_t value=queue.items[queue.head];
    queue.head=(queue.head+1)%queue.capacity;
    queue.count--;

    pthread_cond_signal(&queue.cond_not_full);
    pthread_mutex_unlock(&queue.mutex);

    return value;
}

static const char *mime_type(const char *path){
    const char *dot=strrchr(path,'.');
    if(!dot) return "application/octet-stream";
    if(strcmp(dot,".html")==0) return "text/html";
    if(strcmp(dot,".css")==0) return "text/css";
    if(strcmp(dot,".js")==0) return "application/javascript";
    if(strcmp(dot,".png")==0) return "image/png";
    if(strcmp(dot,".jpg")==0 || strcmp(dot,".jpeg")==0) return "image/jpeg";
    if(strcmp(dot,".gif")==0) return "image/gif";
    return "text/plain";
}

int should_keep_alive(char *headers){
    char *conn=strstr(headers,"Connection:");
    if(conn==NULL) return 1;

    conn+=strlen("Connection:");
    while(*conn==' ') conn++;

    if(strncasecmp(conn,"close",5)==0)
        return 0;

    return 1;
}

void log_request(const char *client_ip,const char *method,const char *path,int status_code,size_t bytes_sent){
    time_t now=time(NULL);
    struct tm *tm_info=localtime(&now);

    char timestamp[64];
    strftime(timestamp,sizeof(timestamp),"%d/%b/%Y:%H:%M:%S %z",tm_info);

    FILE *log_file=fopen("access.log","a");
    if(log_file){
        fprintf(log_file,"%s - - [%s] \"%s %s HTTP/1.1\" %d %zu\n",
        client_ip,timestamp,method,path,status_code,bytes_sent);
        fclose(log_file);
    }
}

void handle_connection(int client_fd,const char *client_ip){

    struct timeval timeout;
    timeout.tv_sec=TIMEOUT_SECONDS;
    timeout.tv_usec=0;
    setsockopt(client_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));

    while(1){

        char buffer[BUFFER_SIZE];
        int total_received=0;

        while(total_received<BUFFER_SIZE-1){
            int bytes=recv(client_fd,buffer+total_received,BUFFER_SIZE-total_received-1,0);
            if(bytes<=0) break;

            total_received+=bytes;
            buffer[total_received]='\0';

            if(strstr(buffer,"\r\n\r\n")) break;
        }

        if(total_received<=0){
            close(client_fd);
            return;
        }

        int keep_alive=should_keep_alive(buffer);

        char *line_end=strstr(buffer,"\r\n");
        if(!line_end){
            close(client_fd);
            return;
        }

        char request_line[256];
        int len=line_end-buffer;
        strncpy(request_line,buffer,len);
        request_line[len]='\0';

        char method[16],path[256],version[16];
        sscanf(request_line,"%15s %255s %15s",method,path,version);

        if(strstr(method,"POST")){

            char *content_length_ptr=strstr(buffer,"Content-Length:");
            if(!content_length_ptr){
                close(client_fd);
                return;
            }

            content_length_ptr+=strlen("Content-Length:");
            while(*content_length_ptr==' ') content_length_ptr++;

            int content_length=atoi(content_length_ptr);

            char *body_start=strstr(buffer,"\r\n\r\n")+4;
            int body_bytes_in_buffer=total_received-(body_start-buffer);

            char body[BUFFER_SIZE];
            memset(body,0,BUFFER_SIZE);

            if(body_bytes_in_buffer>0){
                memcpy(body,body_start,body_bytes_in_buffer);
            }

            int bytes_read=body_bytes_in_buffer;

            while(bytes_read<content_length){
                int n=recv(client_fd,body+bytes_read,content_length-bytes_read,0);
                if(n<=0) break;
                bytes_read+=n;
            }
	    const char *prefix="You sent: ";
            char response[BUFFER_SIZE];
            int resp_len=snprintf(response,sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %ld\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: %s\r\n"
                "\r\n"
                "%s%s",
                bytes_read+strlen(prefix),
                keep_alive?"keep-alive":"close",
		prefix,
                body);

            send(client_fd,response,resp_len,0);

            if(!keep_alive){
                close(client_fd);
                return;
            }

            continue;
        }

        if(strstr(path,"..")){
            const char *msg="HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
            send(client_fd,msg,strlen(msg),0);
            close(client_fd);
            return;
        }

        char filepath[PATH_MAX];
        snprintf(filepath,sizeof(filepath),"./public%s",path);

        if(strcmp(path,"/")==0)
            strcpy(filepath,"./public/index.html");

        FILE *file=fopen(filepath,"rb");

        if(!file){
            const char *msg="HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
            send(client_fd,msg,strlen(msg),0);
            close(client_fd);
            return;
        }

        struct stat st;
        stat(filepath,&st);
        long file_size=st.st_size;

        char headers[256];
        int header_len=snprintf(headers,sizeof(headers),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: %ld\r\n"
            "Content-Type: %s\r\n"
            "Connection: %s\r\n"
            "\r\n",
            file_size,
            mime_type(filepath),
            keep_alive?"keep-alive":"close");

        send(client_fd,headers,header_len,0);

        size_t n;
        while((n=fread(buffer,1,BUFFER_SIZE,file))>0){
            send(client_fd,buffer,n,0);
        }

        fclose(file);

        log_request(client_ip,method,path,200,file_size);

        if(!keep_alive){
            close(client_fd);
            return;
        }
    }
}

void *worker_thread(void *arg){
    int id=*(int*)arg;
    printf("worker %d started\n",id);

    while(1){
        connection_t conn=dequeue();
        handle_connection(conn.client_fd,conn.client_ip);
    }
}

int main(){

    queue_init(&queue,QUEUE_SIZE);

    int thread_ids[NUM_THREADS];

    for(int i=0;i<NUM_THREADS;i++){
        thread_ids[i]=i;
        pthread_create(&workers[i],NULL,worker_thread,&thread_ids[i]);
    }

    int server_fd=socket(AF_INET,SOCK_STREAM,0);

    int opt=1;
    setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in addr={
        .sin_family=AF_INET,
        .sin_port=htons(PORT),
        .sin_addr.s_addr=INADDR_ANY
    };

    bind(server_fd,(struct sockaddr*)&addr,sizeof(addr));
    listen(server_fd,100);

    printf("Serving ./public on http://localhost:%d\n",PORT);

    while(1){

        struct sockaddr_in client_addr;
        socklen_t client_len=sizeof(client_addr);

        int client_fd=accept(server_fd,(struct sockaddr*)&client_addr,&client_len);
        if(client_fd<0) continue;

        connection_t conn;
        conn.client_fd=client_fd;

        inet_ntop(AF_INET,&client_addr.sin_addr,
                  conn.client_ip,sizeof(conn.client_ip));

        enqueue(conn);
    }

    close(server_fd);
}
