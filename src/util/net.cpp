#include <iostream>
#include <string>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <netdb.h>


#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "net.h"

using namespace std;


void get_ip_address(struct in_addr *ip_addr) {

    char host_buffer[256];

    // retrive host name
    int hostname = gethostname(host_buffer, sizeof(host_buffer));
    if (hostname < 0) {
        perror("gethostname failed\n");
        inet_aton("127.0.0.1", ip_addr);
        return;
    }

    // retrieve host information
    struct hostent *host_entry = gethostbyname(host_buffer);
    if (host_entry == NULL) {
        perror("gethostname failed!!");
        inet_aton("127.0.0.1", ip_addr);
        return;
    }
    
    printf("num_interfaces : %d\n", host_entry->h_length);
    // printf("test1");
    struct in_addr *host_addr = ((struct in_addr*)host_entry->h_addr_list[0]);
    memcpy(ip_addr, ip_addr, sizeof(struct in_addr));

    char *ip_buffer = inet_ntoa(*host_addr);
    printf("ip : %s", ip_buffer);

    return;
}


int create_server(short port) {

    int sockfd;
    struct sockaddr_in servaddr;

    // socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("socket creation failed..");
        return -1;
    }

    // assign ip, port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    //bind to port
    if(bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        // printf("failed to bind to server port %d");
        std::cout << "failed to bind to port " << port << std::endl;
        return -1;
    }

    return sockfd;
}

int stop_server(int sockfd) {
    close(sockfd);
}

int connect_to_server(char *server_ip_str, short server_port) {

    // create a socket to contact hub
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Unable to create hub socket\n");
        return -1;
    }
    
    //
    printf("connecting to (%s:%d)\n", server_ip_str, (int)server_port);

    struct sockaddr_in serv_addr;
    serv_addr.sin_family	= AF_INET;
    inet_aton(server_ip_str, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(server_port);
    
    // connect to hub
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Unable to connect to hub!!");
        return -1;
    }

    return sockfd;
}


int disconnect_from_server(int sockfd) {
    close(sockfd);
}


void parse_ip_addr(char *ip_port, char *ip_addr) {
    
    std::string ip_port_str(ip_port);
    int del_pos = ip_port_str.find(":");
    std::string ip_addr_str = ip_port_str.substr(0, del_pos);

    std::cout << "parsed_ip_address : " << ip_addr_str << std::endl;
    memcpy(ip_addr, ip_addr_str.c_str(), ip_addr_str.size() + 1);

    return;
}

short parse_port(char *ip_port) {

    std::string ip_port_str(ip_port);
    int del_pos = ip_port_str.find(":");
    std::string port_str = ip_port_str.substr(del_pos+1, ip_port_str.size());

    return (short)(std::stoi(port_str));
}

/* read from the fd until the buff contains {size} bytes*/
int read_full(int sockfd, void *buff, int size) {
    
    int offset = 0;
    while (offset < size) {
        int bytes_read = recv(sockfd, buff, size - offset, 0);
        if (bytes_read < 0) {
            return -1;
        }

        offset += bytes_read;
    }

    return offset;
}

/* write entire buff to fd */
int write_full(int sockfd, void *buffer, int size) {

    char *buff = (char *)buffer;
    int offset = 0;
    while (offset < size) {
        int bytes_written = send(sockfd, buff + offset, size - offset, 0);
        if (bytes_written < 0) {
            return -1;
        }
        
        offset += bytes_written;
    }

    return offset;
}

int recv_full(int sockfd, void *buffer, int size) {
    char *buff = (char *)buffer;
    int offset = 0;
    while (offset < size) {
        int bytes_recv = recv(sockfd, buff + offset, size - offset, 0);
        // std::cout << "bytes recv : " << bytes_recv << std::endl;
        if (bytes_recv < 0) {
            perror("peer node failed\n");
            return -1;
        }

        offset += bytes_recv;
    }

    return offset;
}

int send_full(int sockfd, void *buffer, int size) {
    char *buff = (char *)buffer;
    int offset = 0;
    while (offset < size) {
        int bytes_sent = send(sockfd, buff + offset, size - offset, 0);
        // std::cout << "bytes sent : " << bytes_sent << std::endl;
        if (bytes_sent < 0) {
            perror("peer node failed\n");
            return -1;
        }
        
        offset += bytes_sent;
    }

    return offset;
}