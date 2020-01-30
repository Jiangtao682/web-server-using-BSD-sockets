# CS118 Project 1

Template for for [UCLA CS118 Spring 2019 Project 1]

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` to add your userid for the `.tar.gz` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## Provided Files

`server.c` is the entry points for the server part of the project.

## Report

Jiangtao Chen
UID: 305429047

## Task Description

This project requires us to implement a simple web server in the virtual machine using BSD sockets. This simple web server can receive the connect request from host browser and process the http massage from the client. After build the connection successfully, the server need to extract filename that requested by the client. after that, the server makes the filename case-insensitive and sends corresponding file to client with appropriate headers.

I am using Linux OS running in the VM to build the server. Chrome as the client runs in the same computer. To send connect requests with browser we can  just use the URL:

- localhost:8080/filename. Extension

## High Level Design

The skeleton of my web server refer to the code in [Guide to Network Programming Using Sockets]( http://beej.us/guide/bgnet/ ). The port number is assigned by vagrant file, which is guest: 8000, host: 8080. 

To build the web server, there are five major steps to take. The first step is to create a socket with the socket() system call. Before we call the function socket(), we need to initialize the data structure with getaddrinfo() function which can help us set up the structs we need later on.  Then the server bind the socket to IP address number and port number using the bind() system call. Listen() function is to set your server ready to receive connection request from any client. When there are some clients request for connection, the server initial a new process to handle the communication.

After our server accept one connection, we get a new socket file descriptor for this single connection. By tokenizing the descriptor, we extract the file name requested by the client. The tokenization is finished in the function of request_process(), which return a pointer points to the filename. Then I call the prepareFile (int, char *) to load the file in to buffer, create the http header and send these massages and file to client. 

The header massage is important because it tells client the type of file so the client's browser knows how to display the file. Whenever the server finds an error it sends a "404 not found" massage to client. 

## Problems and Solutions 

Although the  [Guide to Network Programming Using Sockets]( http://beej.us/guide/bgnet/ ) makes it easer to implement a web server on the Linux system, the structs are confusing and many struct are very similar to each other. The C syntax and the usage of pointer is a little bit tricky for me at the beginning stage.

Creating the http header is also challenging because this part is not covered in the Guide book. The header struct need to fit well to the standard format, and even the smallest inconsistency or error in the line makes the connection fail. Debugging is also difficult when I run program in the VM. I can only use printf() to help me make sure I got desired results in every code blocks. 

## Libraries used

```
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
```

## Acknowledgement

Thanks these content contributors for providing the relative information on the Internet. I referred to the online resources below.

 [Guide to Network Programming Using Sockets]( http://beej.us/guide/bgnet/ ). p30-p33

```c
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

int main(void)
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

	freeaddrinfo(servinfo); // all done with this structure

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
			if (send(new_fd, "Hello, world!", 13, 0) == -1)
				perror("send");
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

```

 https://www.binarytides.com/socket-programming-c-linux-tutorial/ 

 https://medium.com/from-the-scratch/http-server-what-do-you-need-to-know-to-build-a-simple-http-server-from-scratch-d1ef8945e4fa  

























