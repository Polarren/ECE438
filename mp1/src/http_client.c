/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "80" // the port client will be connecting to 

#define MAXDATASIZE 4096 // max number of bytes we can get at once 
#define MAXDOMAINSIZE 4096 // max number of bytes of a domain

struct HTTP_RES //Struct for HTTP response 
{
    int status_code;//HTTP/1.1 '200' OK
    char content_type[128];//Content-Type: application/gzip
    long content_length;//Content-Length: 11683079
	char *data;
};

struct HTTP_RES parse_response(char *response){
	struct HTTP_RES resp;
	memset(&resp, 0, sizeof(resp));

    char *pos = strstr(response, "HTTP/");
    if (pos)//Get status code
        sscanf(pos, "%*s %d", &resp.status_code);

	pos = strstr(response, "Content-Type:");
    if (pos)//get content type
        sscanf(pos, "%*s %s", resp.content_type);

    pos = strstr(response, "Content-Length:");
    if (pos)//get content length
        sscanf(pos, "%*s %ld", &resp.content_length);

	pos = strstr(response, "\r\n\r\n");
    if (pos)//get data
        resp.data = pos+4;
	return resp;
}

void parse_URL(char* input, char* hostname, char* path_to_file, char*port){

	char domain[MAXDOMAINSIZE];
	char* url=NULL;
	int domain_len;

	// Get rid of http
	if (strncmp(input, "https://",8)==0 || strncmp(input, "HTTPS://",8)==0)
		url = input+8;
	else if ((strncmp(input, "http://",7)==0 || strncmp(input, "HTTP://",7)==0))
		url = input+7;
	else
		url = input;
	// Find domain and port 
	char* first_slash = strchr(url,'/');
	if (!first_slash) { //no slash in url: url is the domain
		strcpy(domain,url); 
	} else {
		domain_len = first_slash-url;
		strncpy(domain, url,domain_len);
		domain[domain_len]='\0';
		if (strlen(url+domain_len) >1)  // in case only "/" follows the domain
			strcpy(path_to_file,url+domain_len);  // found path to file	
	}
	// Find hostname and port
	char* colon;
	colon = strchr(domain,':');
	if (colon) {
		int hostname_len =colon- domain;
		domain[hostname_len]='\0'; 
		strcpy(hostname,domain); // found hostname
		strcpy(port,colon+1);  // found port
	} else {
		strcpy(hostname,domain); // found hostname
		
	}
	
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, ret;  
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char str1[MAXDATASIZE];

	if (argc != 2) {
	    fprintf(stderr,"No hostname specified\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* **************************** Parse Input ********************************* */
	char *URL = argv[1];
	char port[8]; //defualt port
	char hostname[MAXDOMAINSIZE],path_to_file[MAXDATASIZE];
	memset(port,0,8);
	memset(hostname,0,MAXDOMAINSIZE);
	memset(path_to_file,0,MAXDATASIZE);
	strcpy(port,"80");//default port
	strcpy(hostname,"127.0.0.1");//default hostname
	strcpy(path_to_file,"/index.html");//default file
	parse_URL(URL, hostname, path_to_file, port);

	// printf("hostname is :%s\n",hostname);
	// printf("port is :%s\n",port);
	// printf("filepath is :%s\n",path_to_file);
	if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	/* **************************** GET Request ********************************* */

    memset(str1, 0, MAXDATASIZE);
    strcat(str1, "GET ");
	strcat(str1, path_to_file);
	strcat(str1, " HTTP/1.1\r\n");
	strcat(str1, "User-Agent: Wget/1.12 (linux-gnu)\r\n");
    strcat(str1, "Host: ");
	strcat(str1, hostname);
    strcat(str1, "\r\nConnection: close\r\n");

    strcat(str1, "\r\n");
    printf("%s\n",str1);

    ret = send(sockfd,str1,strlen(str1),0);
    if (ret < 0) {
            printf("GET failed with error code %d and error message'%s'\n",errno, strerror(errno));
            exit(0);
    }else{
            printf("Send success，%d bytes are sent.\n\n", ret);
    }

	/* ****************************  GET Response  **********************************/

	char buf[MAXDATASIZE];
	int mem_size = MAXDATASIZE;
	int len;
	int length = 0;

	char *response = (char *) malloc(mem_size * sizeof(char));
	response[0] = '\0';

	while ((len = recv(sockfd, buf, MAXDATASIZE-1,0)) != 0)
    {
		if (len < 0) {
            printf("recv() failed with error code %d and error message'%s'\n",errno, strerror(errno));
			exit(-1);
		}

		// printf("Received data from server");
        if (length + len >= mem_size)
        {
            //Memory reallocation, incase buf is not enough
            mem_size *= 2;
			// printf("Reallocating memory for response.\n");
            char * temp = (char *) realloc(response, sizeof(char) * mem_size);
            if (temp == NULL)
            {
                printf("Failed to reallocate memory for HTTP response.\n");
                exit(-1);
            }
            response = temp;
        }

        buf[len] = '\0';
        strcat(response, buf);

        length += len;
    }
	// printf("The HTTP response is:\n%s\n",response);

    struct HTTP_RES resp = parse_response(response);

    if (resp.status_code != 200)
    {
        printf("Download failed: %d\n", resp.status_code);
    } else{

	char *file_name = "output";
	FILE *fd = fopen(file_name, "w");
	fputs( resp.data, fd);
    
	printf("File write finished!\n");
	fclose(fd);
	}
	// free(response);
	
	close(sockfd);

	return 0;
}

