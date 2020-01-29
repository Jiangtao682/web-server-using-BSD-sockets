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

#define PORT "8000"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

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
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
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
		new_fd = accept(sockfd, (struct sockaddr *)&clients_addr, &sin_size);
		if (new_fd == -1) {
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
            char buffer[512]; //Read 512 characters every time
            bzero(buffer,512);
            if (read(new_fd, buffer, 511) < 0)//Reading message to buffer
                fprintf(stderr, "Socket reading error\n");
            printf("HTTP Request Message:\n%s\n", buffer);
            
            //Tokenize the received message
            const char space[2] = " ";
            filename = strtok(buffer, space);
            filename = strtok(NULL, space);
            //Delete first character '/'
            filename++;
            
            if(strlen(filename)<=0) filename = "\0";
            printf("Request file: %s\n", filename);
            
            
            //----------------- Serve Files ------------------
            char* err404 = "HTTP/1.1 404 Not Found\r\n\r\n";
            char* err404_html = "<h1>Error 404: File Not Found!</h1> <br><br>";
            if(strncmp(filename, "\0", 1) == 0)
            {
                send(new_fd, err404, strlen(err404), 0);
                send(new_fd, err404_html, strlen(err404_html), 0);
                printf("No file specified\n");
                close(new_fd);
                exit(0);
            }
            
            // space_replace(filename);
            
            // FILE *fd = fopen(filename, "r");
            // if (fd==NULL)
            // {
            //     send(newfd, err404, strlen(err404), 0);
            //     send(newfd, err404_html, strlen(err404_html), 0);
            //     printf("File not found\n");
            //     close(newfd);
            //     exit(0);
            // }
            
            // char *content = NULL;
            // if (fseek(fd, 0L, SEEK_END) == 0)
            // {
            //     long file_size = ftell(fd);
            //     if (file_size == -1)
            //     {
            //         send(newfd, err404, strlen(err404), 0);
            //         send(newfd, err404_html, strlen(err404_html), 0);
            //         printf("File size error\n");
            //         close(newfd);
            //         exit(0);
            //     }
                
            //     //allocate content buffer
            //     content = malloc(sizeof(char) * (file_size + 1));
                
            //     if (fseek(fd, 0L, SEEK_SET) != 0)
            //     {
            //         send(newfd, err404, strlen(err404), 0);
            //         send(newfd, err404_html, strlen(err404_html), 0);
            //         printf("File size error\n");
            //         close(newfd);
            //         exit(0);
            //     }
                
            //     //read content to buffer
            //     size_t content_size = fread(content, sizeof(char), file_size, fd);
                
            //     //check read process
            //     if (content_size == 0)
            //     {
            //         send(newfd, err404, strlen(err404), 0);
            //         send(newfd, err404_html, strlen(err404_html), 0);
            //         printf("File size error\n");
            //         close(newfd);
            //         exit(0);
            //     }
                
            //     //set terminal character
            //     content[content_size] = '\0';
            //     response(newfd, filename, content_size);
            //     send(newfd, content, content_size, 0);
            //     printf("File served: \"%s\"\n\n", filename);
            // }
            
            // // close file and free dynamically allocated file source
            // fclose(fd);
            // free(content);
            
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    return 0;
}


