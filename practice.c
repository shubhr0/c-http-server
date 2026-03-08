#include<stdlib.h>
#include<string.h>
#include<stdio.h>

char *buffer = "POST /submit HTTP/1.1\r\n"
               "Host: localhost\r\n"
               "Content-Length: 100\r\n"
               "\r\n";



int get_content_length(char *headers)
{
	
	char *myptr=strstr(headers,"Content-Length:");
	if(myptr == NULL){
		return -1;
	}
	myptr +=strlen("Content-Length:");
	while(*myptr == ' '){
		myptr++;
	}
	int length=atoi(myptr);
	return length;
}
int main()
{
	int len=get_content_length(buffer);
	printf("%d\n",len);
}

