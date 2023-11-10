#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#ifdef UNIX_SOCKET
#define SOCKET_TYPE AF_UNIX
#define SOCKET_PATH "/tmp/my_unix_socket"
#else
#define SOCKET_TYPE AF_INET
#define SERVER_IP "10.0.0.101"
#define SERVER_PORT 9101
#endif

bool jtt1078_on_msg(const char* msg)
{
	//ip:127.0.0.1 udpport:0 tcpport:6802 logicalchannel:1 datatype:0 streamchannel:1 
 	printf(">>on msg: %s\n", msg);
     char ipStr[16]; // Buffer for IP address
    int tcpPort = 0;
    // Find the "ip:" substring in the input
    const char *ipPtr = strstr(msg, "ip:");
    if (ipPtr != NULL) {
        // Read the IP address
        if (sscanf(ipPtr, "ip:%15[^ ]", ipStr) != 1) {
            // Handle error
            printf("Failed to parse IP address.\n");
            
        }
    } else {
        // Handle error
        printf("'ip:' not found in the string.\n");
        
    }
        // Find the "tcpport:" substring in the input
    const char *portPtr = strstr(msg, "tcpport:");
    if (portPtr != NULL) {
        // Read the TCP port
        if (sscanf(portPtr, "tcpport:%d", &tcpPort) != 1) {
            // Handle error
            printf("Failed to parse TCP port.\n");
           
        }
    } else {
        // Handle error
        printf("'tcpport:' not found in the string.\n");
        
    }
    printf("ip@%s port @%d \n" , ipStr, tcpPort);
    sendJTT1078Stream(ipStr, tcpPort);
    return true;
}

int main()
{
	int server_fd, client_fd;
    struct sockaddr_un server_addr_unix;
    struct sockaddr_in server_addr_inet;
    struct sockaddr client_addr; // Declare client_addr here
    struct sockaddr *server_addr; // Declare server_addr here
    socklen_t client_len;
    char buffer[256];

    // Create socket
    server_fd = socket(SOCKET_TYPE, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd == -1) {
        printf("Socket creation failed.\n");
        return false;
    }

#ifdef UNIX_SOCKET
    // Setup Unix socket server address structure
    memset(&server_addr_unix, 0, sizeof(struct sockaddr_un));
    server_addr_unix.sun_family = AF_UNIX;
    strncpy(server_addr_unix.sun_path, SOCKET_PATH, sizeof(server_addr_unix.sun_path) - 1);
    server_addr = (struct sockaddr*)&server_addr_unix;
#else
    // Setup TCP/IP socket server address structure
    memset(&server_addr_inet, 0, sizeof(struct sockaddr_in));
    server_addr_inet.sin_family = AF_INET;
    server_addr_inet.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr_inet.sin_port = htons(SERVER_PORT);
    server_addr = (struct sockaddr*)&server_addr_inet;
    //use TIME_WAIT state
    int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    	printf("setsockopt error.\n");
	}

#endif

    // Bind the socket to the address
    if (bind(server_fd, server_addr, sizeof(*server_addr)) == -1) {
        printf("Socket bind failed.\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) == -1) {
        printf("Socket listen failed.\n");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening...\n");

	client_len = sizeof(client_addr);
    while (1) {
        client_fd = accept4(server_fd, &client_addr, &client_len, 0); // Use &client_addr
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No incoming connection, continue
                usleep(10000); // Sleep for a short while to avoid busy-waiting
                continue;
            } else {
                printf("Accept failed. errno %d server_fd %d\n", errno, server_fd);
                continue;
            }
        }
        else {
        	printf("got new connection. %d\n", client_fd);
        	break;
        }
	}

	while (1) {
        // Handle incoming data (non-blocking)
        ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, continue
                continue;
            } else {
                printf("Receive failed\n");
            }
        } else if (bytes_received == 0) {
            printf("Client disconnected\n");
        } else {
            buffer[bytes_received] = '\0'; // Null-terminate received data
            printf("Received message: %s\n", buffer);
            jtt1078_on_msg(buffer);
            
        }
    }

    close(server_fd); // Close the server socket

    return 0;
}

