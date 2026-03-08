#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>


//keep reading until we see \r\n\r\n
//check if we have received the end of the headers
//find the end of the first line
//make a copy of the request line
//now parse it into three parts
//start after the request line
//find where header end(\r\n\r\n)
//parse each header line


#define PORT 8080
#define BUFFER_SIZE 4086

void parse_request(int client_fd)
{
	char buffer[BUFFER_SIZE];
	int total_received=0;

	//read until we get \r\n\r\n
	
	while(total_received<BUFFER_SIZE -1){
		int bytes=recv(client_fd,buffer+total_received,BUFFER_SIZE-total_received -1,0);
		if(bytes<=0) break;

		total_received+=bytes;
		buffer[total_received] ='\0';

		if(strstr(buffer,"\r\n\r\n")!=NULL) break;
	}

	printf("\n=== received request ===\n");


	//parse request line
	char *line_end=strstr(buffer,"\r\n");
	if(line_end == NULL){
		printf("invalid request");
		return;
	}

	int line_length=line_end-buffer;
	char request_line[256];
	strncpy(request_line,buffer,line_length);
	request_line[line_length]='\0';

	char method[16],path[256],version[16];
	sscanf(request_line, "%s %s %s",method,path,version);

	printf("method: %s\n",method);
	printf("path: %s\n",path);
	printf("version: %s\n",version);

	//parse headers
	

	char *headers_start = line_end +2;
	char *headers_end = strstr(buffer,"\r\n\r\n");

	printf("\nHeaders:\n");
	char *line=headers_start;
	while(line< headers_end){
		char *next_line=strstr(line,"\r\n");
		if(next_line ==NULL) break;

		int len=next_line-line;
		char header_line[512];
		strncpy(header_line,line,len);
		header_line[len]='\0';

		char *colon =strchr(header_line,':');
		if(colon!=NULL){
			*colon ='\0';
			char *key=header_line;
			char *value = colon +1;
			while(*value == ' ') value++;
			printf(" %s:%s\n",key,value);
		}

		line=next_line+2;
	}

	

	printf("====================\n\n");
}

void send_response(int client_fd)
{
	char *body="Hello World";
	int body_length=strlen(body);

	char response[BUFFER_SIZE];

	sprintf(response,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type:text/html\r\n"
		"Content-Length:%d\r\n"
		"\r\n"
		"%s",
		body_length,
		body
	);


	send(client_fd,response,strlen(response),0);
}
		
		


int main()
{
	int server_fd=socket(AF_INET,SOCK_STREAM,0);

	int opt = 1;
	setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

	struct sockaddr_in address;
	address.sin_family =AF_INET;
	address.sin_addr.s_addr= INADDR_ANY;
	address.sin_port= htons(PORT);

	bind(server_fd,(struct sockaddr *)&address,sizeof(address));
	listen(server_fd,10);

	printf("server listening on port %d\n",PORT);

	while(1){
		int client_fd = accept(server_fd,NULL,NULL);
		printf("\n==== new connection ====\n");


		parse_request(client_fd);
		
		send_response(client_fd);
			
		close(client_fd);
	}

	return 0;

}

