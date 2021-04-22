
#ifndef _NETCALLS_H_
#define _NETCALLS_H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>

// void get_ip_address(struct in_addr *ip_addr);

/* utility functions to create and stop a server*/
int create_server(short port);
int stop_server(int sockfd);

/* functions to connect to and disconnect from a server */
int connect_to_server(char *ip_addr_str, short server_port);
int disconnect_from_server(int sockfd);

/* input is string ipaddr:port */
void parse_ip_addr(char *ip_port_str, char *ip_addr_str);
short parse_port(char *ip_port);

/* some network read utilities */
// int read_full(int sockfd, void *buff, int size);
// int write_full(int sockfd, void *buff, int size); 

int recv_full(int sockfd, void *buff, int size);
int send_full(int sockfd, void *buff, int size);

#endif