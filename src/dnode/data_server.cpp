#include <iostream>
#include <fstream>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <vector>

#include "dnode.h"
#include "data_server.h"

#include "../hub/hub.h"
#include "../util/hash.h"
#include "../util/net.h"
#include "../util/fs.h"

using namespace std;

extern struct dnode_details_struct dnode_details;

void handle_data_transfer(int peer_sockfd) {

    // get file data request
    struct file_data_req_struct file_data_req;
    recv_full(peer_sockfd, &file_data_req, sizeof(file_data_req));
        
    // parse file data response
    int offset = file_data_req.offset;
    int size = file_data_req.size;
    
    printf("req_file_offset : %d\n", offset);
    printf("req_flle_chunk_size :  %d\n", size);

    string files_dir = string(dnode_details.files_dir); 
    string file_path = files_dir + string("/") + string(file_data_req.file_name);

    int file_size = get_file_size(file_path.c_str());

    printf("file_path : %s\n", file_path.c_str());
    printf("file size : %d\n", file_size);

    // fill response
    struct file_data_res_struct file_data_res;
    if (offset < 0 || offset + size > file_size) { // can not server data request
        printf("error : size + offset > file_size\n");
        file_data_res.res_type = DATA_TRANSFER_FAILURE;
        file_data_res.payload_len =  0;    
        send_full(peer_sockfd, &file_data_res, sizeof(file_data_res));
        close(peer_sockfd);
        return;
    }

    // std::cout << "file_size : " << file_size << std::endl;
    // std::cout << "offset : " << offset << std::endl;
    // std::cout << "size : " << size << std::endl;

    file_data_res.res_type = DATA_TRANSFER_SUCCESS;
    file_data_res.payload_len = size;

    // open the requested file 
    int read_fd = open(file_path.c_str(), O_RDONLY);
    if (read_fd < 0) {
        perror("file opening error\n");
        file_data_res.res_type = DATA_TRANSFER_FAILURE;
        file_data_res.payload_len =  0;    
        send_full(peer_sockfd, &file_data_res, sizeof(file_data_res));
        close(peer_sockfd);
        return;
    }

    // set the offset to the file
    lseek(read_fd, offset, SEEK_SET);

    // send the entire requested chunk in one go
    uint8_t *chunk_data = (uint8_t *)malloc(size);
    int bytes_read = fread_full(read_fd, chunk_data, size);
    printf("chunk checksum : %08lx\n", compute_hash(chunk_data, size));
    int bytes_sent = send_full(peer_sockfd, chunk_data, size);
    free(chunk_data);

    std::cout << "bytes read (fs) : " << bytes_read << std::endl;
    std::cout << "bytes sent (net): " << bytes_sent << std::endl;
    
    close(read_fd);

}


void *socket_handler_threaded(void *args) {
    struct dt_thread_args_struct *thread_args;
    thread_args = (struct dt_thread_args_struct *)args;
    handle_data_transfer(thread_args->sockfd);

    close(thread_args->sockfd);
}


void *handle_data_server(void *args) {

    std::cout << "Data server :: starting !!" << std::endl;
    uint8_t *data_buffer = (uint8_t *)malloc(2048);
    int data_port = dnode_details.dnode_data_port;
    int sockfd = create_server(data_port);

    if(sockfd < 0) {
        std::cout << "Data server :: Unable to create server with port " << data_port << std::endl;
        std::cout << "Data server :: Terminating.. " << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Data server :: server created!!" << std::endl;

    if (listen(sockfd, 5) == 0) {
        std::cout << "Data server :: listening for data requests on port " << data_port << std::endl;
    }

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    while(1) {
        
        int peer_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (peer_sockfd < 0) {
            perror("Accept error\n");
            std::exit(0);
        }

        std::cout << "\nData server :: Handling file data request " << std::endl;

        // handle_data_transfer(peer_sockfd);

        pthread_t tid;
        int args_size = sizeof(struct dt_thread_args_struct);
        struct dt_thread_args_struct *thread_args = (struct dt_thread_args_struct *)malloc(args_size);
        thread_args->sockfd = peer_sockfd;

        if (pthread_create(&tid, NULL, socket_handler_threaded, thread_args) != 0) {
            perror("failed to spawn a new data transfer thread");
        }
        
        // close(peer_sockfd);
    }

    close(sockfd);
}
