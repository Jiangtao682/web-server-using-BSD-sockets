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

#define HTML "Content-Type: text/html\r\n"
#define TXT "Content-Type: text/plain\r\n"
#define JPEG "Content-Type: image/jpeg\r\n"
#define JPG "Content-Type: image/jpg\r\n"
#define GIF "Content-Type: image/gif\r\n"

#define STATUS_200 "HTTP/1.1 200 OK\r\n"
#define STATUS_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define ERROR_404_HTML "<h1>Error 404: File Not Found!</h1> <br><br>"


char *request_process(int);
void prepareFile (int, char *);
void generateResponseMessage(int, char *, size_t);

void space_replace(char *filename)
{
    char buffer[1024] = {0};
    char *insert_point = &buffer[0];
    const char *tmp = filename;
    
    while (1) {
        const char *p = strstr(tmp, "%20");
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;
        memcpy(insert_point, " ", 1);
        insert_point += 1;
        tmp = p + 3;
    }
    // write altered string back to target
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
	hints.ai_family = AF_UNSPEC; //don't care ipv4 or 6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // fill in my ip for me

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

char *request_process(int socket_fd)
{
// return requested filename
	char buffer[512]; //Read 512 characters every time
	bzero(buffer,512); // clear whole mem space
	if (read(socket_fd, buffer, 511) < 0)//Reading message to buffer
		fprintf(stderr, "Socket reading error\n");
	printf("HTTP Request Message:\n%s\n", buffer);
	char *filename;
	//Tokenize the received message
	const char space[2] = " ";
	filename = strtok(buffer, space);
	filename = strtok(NULL, space);
	//Delete first character '/'
	filename++;
	
	if(strlen(filename)<=0) filename = "\0";
	printf("Request file: %s\n", filename);

	return filename;
}

void prepareFile(int sock, char *filename)
{
	if(filename=="\0")
	{
		// send 404 error status to client browser, then display error page
		send(sock, STATUS_404, strlen(STATUS_404), 0);
		send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
		printf("Error: no file specified!\n");
		return;
	}

	// create source buffer to fopen and fread designated file
	char *source = NULL;
	char *temp_filename = malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(temp_filename, filename);
	space_replace(temp_filename);
	FILE *fp = fopen(temp_filename, "r");

	if (fp==NULL)
	{
		// send 404 error status to client browser, then display error page
		send(sock, STATUS_404, strlen(STATUS_404), 0);
		send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
		printf("Error: file not found!\n");
		return;
	}

	if (fseek(fp, 0L, SEEK_END) == 0)
	{
		// set fsize to file size
		long fsize = ftell(fp);
		if (fsize == -1)
		{
			// send 404 error status to client browser, then display error page
			send(sock, STATUS_404, strlen(STATUS_404), 0);
			send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
			printf("File size error!\n");
			return;
		}

		// allocate source buffer to filesize
		source = malloc(sizeof(char) * (fsize + 1));

		// return to front of file
		if (fseek(fp, 0L, SEEK_SET) != 0)
		{
			// send 404 error status to client browser, then display error page
			send(sock, STATUS_404, strlen(STATUS_404), 0);
			send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
			printf("File size error!\n");
			return;
		}

		// set source to file data
		size_t sourceLength = fread(source, sizeof(char), fsize, fp);

		// check file source for fread errors
		if (sourceLength == 0)
		{
			// send 404 error status to client browser, then display error page
			send(sock, STATUS_404, strlen(STATUS_404), 0);
			send(sock, ERROR_404_HTML, strlen(ERROR_404_HTML), 0);
			printf("File reading error!\n");
			return;
		}

		// NULL-terminate the source
		source[sourceLength] = '\0';
		
		// send HTTP response header to client browser
		generateResponseMessage(sock, temp_filename, sourceLength);

		// send file to client browser
		send(sock, source, sourceLength, 0);

		printf("File \"%s\" served to client!\n\n", temp_filename);
	}

	// close file and free dynamically allocated file source
	fclose(fp);
	free(source);
}

// generates HTTP response message only if file is successfully served (200 OK)
// message holds the HTML-formatted response message (using <br> tags)
// newMessage holds the console-formatted response message (using \r\n characters)
void generateResponseMessage(int sock, char *filename, size_t fileLength)
{
	char message[512];

	// header status
	char *status; 
	status = STATUS_200;
	
	// header connection
	char *connection = "Connection: close\r\n";

	// header date
	struct tm* dateclock;
	time_t now;
	time(&now);
	dateclock = gmtime(&now);
	char nowtime[35];
	strftime(nowtime, 35, "%a, %d %b %Y %T %Z", dateclock);
	//printf("Date time: %s\n", nowtime);
	char date[50] = "Date: ";
	strcat(date, nowtime);
	strcat(date, "\r\n");

	// header server
	char *server = "Server: Jiangtao's VM \r\n";

	// header last-modified
	struct tm* lmclock;
	struct stat attrib;
	stat(filename, &attrib);
	lmclock = gmtime(&(attrib.st_mtime));
	char lmtime[35];
	strftime(lmtime, 35, "%a, %d %b %Y %T %Z", lmclock);
	//printf("Last modified time: %s\n", lmtime);
	char lastModified[50] = "Last-Modified: ";
	strcat(lastModified, lmtime);
	strcat(lastModified, "\r\n");

	// header content-length	
	char contentLength[50] = "Content-Length: ";	
	char len[10];
	sprintf (len, "%d", (unsigned int)fileLength);
	strcat(contentLength, len);
	strcat(contentLength, "\r\n");

	// header content-type
	char* content_type = TXT;
    char *tmp = malloc(sizeof(char) * (strlen(filename) + 1));
    strcpy(tmp, filename);
    int i = 0;
    while (tmp[i]) {
        tmp[i] = tolower(tmp[i]);
        i++;
    }
    if (strstr(tmp, ".html") != NULL)
        content_type = HTML;
    else if (strstr(tmp, ".txt") != NULL)
        content_type = TXT;
    else if (strstr(tmp, ".jpeg") != NULL)
        content_type = JPEG;
    else if (strstr(tmp, ".jpg") != NULL)
        content_type = JPG;
    else if (strstr(tmp, ".gif") != NULL)
        content_type = GIF;

	strcat(message, status);
    strcat(message, connection);
    strcat(message, date);
    strcat(message, server);
    strcat(message, lastModified);
    strcat(message, contentLength);
    strcat(message, content_type);
    strcat(message, "\r\n\0");

	// send response to client browser as header lines
	send(sock, message, strlen(message), 0);

	// send copy of response to console
	printf("HTTP RESPONSE MESSAGE:\n%s\n", message);
}

