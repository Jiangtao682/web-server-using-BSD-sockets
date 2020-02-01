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
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define PORT "8000"   // the port users will be connecting to
#define BACKLOG 10	  // how many pending connections queue will hold

#define JPG "Content-Type: image/jpg\r\n"
#define GIF "Content-Type: image/gif\r\n"
#define PNG "Content-Type: image/png\r\n"
#define HTML "Content-Type: text/html\r\n"
#define TXT "Content-Type: text/plain\r\n"
#define JPEG "Content-Type: image/jpeg\r\n"


#define S_200 "HTTP/1.1 200 OK\r\n"
#define S_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define ERROR_HTML "<h1>Error 404: File Not Found!</h1> <br><br>"


char *request_process(int);
void prepareFile (int, char *);
void createHttpMassage(int, char *, size_t);

void send_error404(int socket_fd_new)
{
	send(socket_fd_new, S_404, strlen(S_404), 0);
	send(socket_fd_new, ERROR_HTML, strlen(ERROR_HTML), 0);
}

void subst_space(char *filename)
{
    char buffer[1024] = {0};
    char *insert_point = &buffer[0];
    const char *dummy = filename;
    for (int i=0; i< strlen(dummy); i = i+1) {
        const char *p = strstr(dummy, "%20");
        if (p == NULL) {
            strcpy(insert_point, dummy);
            break;
        }
        memcpy(insert_point, dummy, p - dummy);
        insert_point += p - dummy;
        memcpy(insert_point, " ", 1);
        insert_point += 1;
        dummy = p + 3;
    }
    strcpy(filename, buffer);
	printf("filename in strip space: %s\n", filename);
	
}

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
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
	int sockfd, newfd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage clients_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);  //make sure the sruct is empty
	hints.ai_family = AF_UNSPEC; //both ipv4 and ipv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // 

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
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

	freeaddrinfo(servinfo); // free the linked list

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

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
		sin_size = sizeof clients_addr;
		newfd = accept(sockfd, (struct sockaddr *)&clients_addr, &sin_size);
		if (newfd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(clients_addr.ss_family,
			get_in_addr((struct sockaddr *)&clients_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			//Process request message
            char *filename;
			filename = request_process(newfd);
			// printf("filename before strip space: %s\n", filename);
			// space_replace(filename);
			// printf("filename after strip space: %s\n", filename);
			prepareFile(newfd, filename);
            close(newfd);
            exit(0);
        }
        close(newfd);
    }
    return 0;
}

char *request_process(int sock_fd)
{
	char buffer[512]; 
	bzero(buffer,512); // clear whole mem space
	if (read(sock_fd, buffer, 511) < 0)
		fprintf(stderr, "Socket reading error\n");
	printf("HTTP Request Message:\n%s\n", buffer);
	char *fn;
	//extract file name from the discriptor file
	const char space[2] = " ";
	fn = strtok(buffer, space);
	fn = strtok(NULL, space);
	//remove  '/'
	fn++;
	
	if(strlen(fn)<=0) fn = "\0";
	printf("HTTP Request file name: %s\n", fn);

	return fn;
}

void prepareFile(int socket_fd_new, char *filename)
{
	if(filename=="\0")
	{
		send_error404(socket_fd_new);
		printf("Error: no file specified!\n");
		return;
	}
	// initial buffer to store file in it and then send to client.
	char *fileBuffer = NULL;
	char *temp_filename = malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(temp_filename, filename);
	for (int i=0; i< strlen(temp_filename); i = i+1){
		temp_filename[i] = tolower(temp_filename[i]);
	}

	subst_space(temp_filename);
	FILE *filePointer = fopen(temp_filename, "r");
		
	if (filePointer==NULL)
	{
		send_error404(socket_fd_new);
		printf("Error: file not found!\n");
		return;
	}

	if (fseek(filePointer, 0L, SEEK_END) == 0)
	{
		// point to the end of the file and get file size
		long fileSize = ftell(filePointer);
		if (fileSize == -1)
		{
			send_error404(socket_fd_new);
			printf("File size error!\n");
			return;
		}
		// initial buffer to store file contents
		fileBuffer = malloc(sizeof(char) * (fileSize + 1));

		// point to the beginning of file
		if (fseek(filePointer, 0L, SEEK_SET) != 0)
		{
			send_error404(socket_fd_new);
			printf("File size error!\n");
			return;
		}

		size_t readFileLength = fread(fileBuffer, sizeof(char), fileSize, filePointer);

		// when the file length is 0, return 404 error page
		if (readFileLength == 0)
		{
			send_error404(socket_fd_new);
			printf("File reading error!\n");
			return;
		}

		// end the buffer
		fileBuffer[readFileLength] = '\0';
		// send HTTP header to client's browser
		createHttpMassage(socket_fd_new, temp_filename, readFileLength);
		// send file 
		send(socket_fd_new, fileBuffer, readFileLength, 0);
		printf("\"%s\" has been served to client!\n\n", temp_filename);
	}
	// close the file and free the buffer
	fclose(filePointer);
	free(fileBuffer);
}

void createHttpMassage(int socket_fd_new, char *filename, size_t fileLength)
{
	char httpMessage[512];

	char *status; 
	status = S_200; //connect success and  display 200 OK!
	
	// get connection status
	char *connection = "Connection: close\r\n";

	// get date
	struct tm* cur_tm_info;
	time_t time_now;
	time(&time_now);
	cur_tm_info = gmtime(&time_now);
	char time_record[52];
	strftime(time_record, 52, "%a,%e %b %G %T GMT", cur_tm_info);
	char date[70] = "Date: ";
	strcat(date, time_record);
	strcat(date, "\r\n");

	// set server name
	char *server = "Server: Jiangtao's VM \r\n";

	// get last-modified time
	struct tm* lmnow;
	struct stat attrib;
	stat(filename, &attrib);
	lmnow = gmtime(&(attrib.st_mtime));
	char lmtime[35];
	strftime(lmtime, 35, "%a, %d %b %Y %T %Z", lmnow);
	char lastModified[50] = "Last-Modified: ";
	strcat(lastModified, lmtime);
	strcat(lastModified, "\r\n");

	// get file content-length	
	char Length[50] = "Content-Length: ";	
	char len[10];
	sprintf (len, "%d", (unsigned int)fileLength);
	strcat(Length, len);
	strcat(Length, "\r\n");

	// get content-type
	char* content_type = TXT;
    char *tmp_name = malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(tmp_name, filename);
    if (strstr(tmp_name, ".html") != NULL)
        content_type = HTML;
    else if (strstr(tmp_name, ".png") != NULL)
        content_type = JPG;
	else if (strstr(tmp_name, ".txt") != NULL)
        content_type = TXT;
    else if (strstr(tmp_name, ".jpg") != NULL)
        content_type = JPG;
    else if (strstr(tmp_name, ".gif") != NULL)
        content_type = GIF;
	else if (strstr(tmp_name, ".jpeg") != NULL)
        content_type = JPEG;
    
	strcat(httpMessage, status);
    strcat(httpMessage, connection);
    strcat(httpMessage, date);
    strcat(httpMessage, server);
    strcat(httpMessage, lastModified);
    strcat(httpMessage, Length);
    strcat(httpMessage, content_type);
    strcat(httpMessage, "\r\n\0");

	send(socket_fd_new, httpMessage, strlen(httpMessage), 0);
	printf("HTTP Response Message:\n%s\n", httpMessage);
}

