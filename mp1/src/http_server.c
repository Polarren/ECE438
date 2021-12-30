/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 4096 // max number of bytes we can get at once 


struct REQUEST {
	int is_valid;
	char req_type[5]; //GET or POST
	char filename[MAXDATASIZE];
};

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void parse_request(char*request, struct REQUEST * req){
	
	
	char req_type[5]; //See if request is "GET ";
	sscanf(request, "%3s", req_type);
	printf("Request type is:%s\n",req_type);
	if (strcmp(req_type,"GET")!=0){
		printf("HTTP request is invalid");
		req->is_valid = 0;
		return ;
	}
	else{
		// printf("HTTP request type is valid\n");
		strcpy(req->req_type, "GET");
		req->is_valid = 1;
	}
		
	memset(req->filename,0,MAXDATASIZE);
	sscanf(request, "%*s %s", req->filename);
	// printf("Filename is :%s\n",req->filename);
    if (strlen(req->filename) <=1){
		printf("HTTP request filepath is invalid\n");
		req->is_valid = 0;
		return ;
	} else {
		strcpy(req->filename,req->filename+1);
		req->filename[strlen(req->filename)]='\0';
	}
	
	return;


}

void bad_request(int client)  
{  
    char buf[1024];  
  
    /*Send bad request response to client */  
    sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");  
    send(client, buf, sizeof(buf), 0);  
    sprintf(buf, "\r\n");  
    send(client, buf, sizeof(buf), 0);  
	return;
}  

void not_found(int client)  
{  
    char buf[1024];  
  
    /* 404 */  
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");  
    send(client, buf, strlen(buf), 0);  
    sprintf(buf, "\r\n");  
    send(client, buf, strlen(buf), 0);  

	return;
}  

void ok(int client)  
{  
    char buf[1024];  
    //(void)filename;  /* could use filename to determine file type */  
  
    /* OK HTTP header */  
    strcpy(buf, "HTTP/1.0 200 OK\r\n");  
    send(client, buf, strlen(buf), 0);  
    strcpy(buf, "\r\n");  
    send(client, buf, strlen(buf), 0);  
}  


/* send_all function from Stackoverflow 
https://stackoverflow.com/questions/13479760/c-socket-recv-and-send-all-data
*/
int send_all(int socket, void *buffer, size_t length)
{
    char *ptr = (char*) buffer;
    while (length > 0)
    {
        int i = send(socket, ptr, length,0);
        if (i < 1) {
			return 0;	
		}
        ptr += i;
        length -= i;
    }
    return 1;
}

void send_file(int client, FILE *f)  
{  
      
	fseek(f, 0, SEEK_END);
	int length = ftell(f);
    /*send file into socket */  
	char buf[length];
    rewind(f); 
	length = fread(buf, 1, length, f);
	buf[length] = '\0';
	send_all(client, buf, length);
    // while (!feof(f))  
    // {   
    //     fgets(buf, sizeof(buf), f);  
    // }  
	return;
}  



int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if (argc != 2) {
	    fprintf(stderr,"No port specified\n");
	    exit(1);
	}
	char * port = argv[1];

	if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			//char buf[MAXDATASIZE];
			char request[MAXDATASIZE];
			request[0] = '\0';
			int len;
			printf("Receiving request...\n");
			
			if ((len = recv(new_fd, request, MAXDATASIZE-1,0)) > 0){
				request[len]='\0';
				// printf("The HTTP request is:\n%s\n",request);
				struct REQUEST req;
				parse_request(request,&req);
				if (req.is_valid ==0){ //invalid request
					bad_request(new_fd);
				}
				else {
					printf("The requested file is:%s\n",req.filename);
					//look for the requested file
					FILE * f = fopen(req.filename, "r");
					if (!f){
						not_found(new_fd);
					}
					else{ // Send file
						ok(new_fd);
						send_file(new_fd,f);
						fclose(f);
					}
					
				}

				
								
			}
			printf("server: connection closed...\n");
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

