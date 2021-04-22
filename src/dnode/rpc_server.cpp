#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <thread>
#include <vector>

#include "dnode.h"
#include "rpc_server.h"

#include "../hub/hub.h"

#include "../util/hash.h"
#include "../util/net.h"
#include "../util/fs.h"


extern struct dnode_details_struct dnode_details;

void handle_file_upload(int rpc_cli_fd, char *file_path) {

    uint8_t chunk_buffer[FILE_CHUNK_SIZE];

    // get file size
    int file_size = get_file_size(file_path);
    std::cout << "in_file_path : " << file_path << std::endl;
    std::cout << "file size : " << file_size << std::endl;

    string file_path_str(file_path);
    string file_name_str = file_path_str.substr(file_path_str.find_last_of("/") + 1, file_path_str.size());
    string out_path = string(dnode_details.files_dir) + string("/") + file_name_str;
    std::cout << "out_file_path : " << out_path << std::endl; 

    int num_complete_chunks = file_size / FILE_CHUNK_SIZE;
    int num_chunks = num_complete_chunks;
    int partial_chunk_size = file_size % FILE_CHUNK_SIZE;
    if (partial_chunk_size > 0) {
        num_chunks += 1;
    }
    std::cout << "chunk size : " << FILE_CHUNK_SIZE << std::endl;
    std::cout << "num chunks : " << num_chunks << std::endl;
    std::cout << "partial chunk size : " << partial_chunk_size << std::endl;

    int file_index_data_size = num_chunks * FILE_CHUNK_HASH_SIZE;
    uint8_t *file_index_data = (uint8_t *)malloc(file_index_data_size);
    uint64_t *chunk_hash_list = (uint64_t *)file_index_data;

    // open read and write fds.
    int read_fd = open(file_path, O_RDONLY);
    int write_fd = open(out_path.c_str(), O_CREAT | O_WRONLY, 0644);

    int bytes_read = 0;
    int bytes_written = 0;
    int total_bytes_read = 0;
    int total_bytes_written = 0;
    uint64_t chunk_hash;

    // read, hash, write each complete chunk
    for (int i = 0; i < num_complete_chunks; i++) {
        
        bytes_read = fread_full(read_fd, chunk_buffer, FILE_CHUNK_SIZE);
        total_bytes_read += bytes_read;

        chunk_hash = compute_hash(chunk_buffer, FILE_CHUNK_SIZE);
        chunk_hash_list[i] = chunk_hash;

        bytes_written = fwrite_full(write_fd, chunk_buffer, FILE_CHUNK_SIZE);
        total_bytes_written += bytes_written;

        printf("i : %02d, chunk_hash : %08lx\n", i + 1, chunk_hash);
    }

    if (partial_chunk_size > 0) {
        // above operations for last partial chunk
        bytes_read = fread_full(read_fd, chunk_buffer, partial_chunk_size);
        total_bytes_read += bytes_read;

        chunk_hash = compute_hash(chunk_buffer, partial_chunk_size);
        chunk_hash_list[num_chunks - 1] = chunk_hash;

        bytes_written = fwrite_full(write_fd, chunk_buffer, partial_chunk_size);
        total_bytes_written += bytes_written;

        printf("i : %02d, chunk_hash : %08lx\n", num_chunks, chunk_hash);
    }
    
    std::cout << "total bytes read : " << total_bytes_read << std::endl;
    std::cout << "total bytes written : " << total_bytes_written << std::endl;

    close(read_fd);
    close(write_fd);

    // compute file hash
    uint64_t file_hash = compute_hash(file_index_data, file_index_data_size);
    printf("file hash : %08lx\n", file_hash);

    // connect to hub
    int hub_sockfd = connect_to_server(dnode_details.hub_ip, dnode_details.hub_port);

    // send hub request (file upload request)
    struct hub_cmd_struct hub_cmd;
    hub_cmd.cmd_type = FILE_UPLOAD;
    send_full(hub_sockfd, &hub_cmd, sizeof(hub_cmd));

    // send the file upload request details
    struct file_upload_req_struct file_upload_req;
    file_upload_req.dnode_uid = dnode_details.uid;
    file_upload_req.file_hash = file_hash;
    file_upload_req.file_size = file_size;
    memcpy(file_upload_req.file_name, file_name_str.c_str(), file_name_str.size() + 1); // include null byte
    file_upload_req.num_chunks = num_chunks;
    file_upload_req.file_index_data_size = file_index_data_size;
    send_full(hub_sockfd, &file_upload_req, sizeof(file_upload_req));

    // send the index data
    send_full(hub_sockfd, file_index_data, file_index_data_size);

    /* TODO : recive response from server */

    

    // disconnect from hub
    disconnect_from_server(hub_sockfd);

    free(file_index_data);

    // send success response to rpc client
    struct rpc_res_struct rpc_res;
    rpc_res.res_type = RPC_RES_SUCCEES;
    rpc_res.payload_len = 0;
    send_full(rpc_cli_fd, &rpc_res, sizeof(rpc_res));

    return;
}


void *download_thread_handler(void *args) {

    pthread_t tid = pthread_self();
    std::cout << "spawned thread tid : " << tid << std::endl;

    struct download_threads_args_struct *threads_args = (struct download_threads_args_struct *)args;

    uint8_t* download_buffer = threads_args->download_buffer;
    int current_chunk_index = threads_args->current_chunk_index;
    int max_chunk_index = threads_args->max_chunk_index;
    int num_peer_nodes = threads_args->num_peer_nodes;
    int num_chunks = threads_args->num_chunks;

    int *num_chunks_downloaded_ptr = threads_args->num_chunks_downloaded_ptr;
    int *num_bytes_downloaded_ptr = threads_args->num_bytes_downloaded_ptr;
    struct download_src_dnode_struct *src_dnodes_list = threads_args->src_dnodes_list;
    struct download_chunk_struct *chunks_list = threads_args->chunks_list;

    pthread_mutex_t *num_chunks_downloaded_lock = threads_args->num_chunks_downloaded_lock;
    pthread_mutex_t *src_dnodes_list_lock = threads_args->src_dnodes_list_lock;
    pthread_mutex_t *chunks_list_lock = threads_args->chunks_list_lock;

    while(1) {

        pthread_mutex_lock(num_chunks_downloaded_lock);
        // printf("\ncheck for downloaded chunks..\n");
        if (*num_chunks_downloaded_ptr >= threads_args->max_chunk_index) {
            printf("all the downloadable chunks are downloaded..\n");
            pthread_mutex_unlock(num_chunks_downloaded_lock);
            break;
        }
        pthread_mutex_unlock(num_chunks_downloaded_lock);

        struct download_chunk_struct  chunk_struct;
        struct download_src_dnode_struct dnode_struct;

        // take an available chunk which is neither downloaded not downloading
        
        pthread_mutex_lock(chunks_list_lock);
        // printf("searching a downloadable chunk..\n");
        int chunk_index = -1;
        for (int i = current_chunk_index; i < max_chunk_index; i++) {
            if (chunks_list[i].is_downloaded == 0 && chunks_list[i].is_downloading == 0) {
                chunk_index = i;
                printf("found downloadable chunk : %d\n", chunk_index);
                memcpy(&chunk_struct, &chunks_list[i], sizeof(chunk_struct));
                chunks_list[chunk_struct.index].is_downloading = 1;
                break;
            }
        }
        if (chunk_index < 0) { // no chunk available to download
            printf("no downloadable chunk found..\n");
            pthread_mutex_unlock(chunks_list_lock);
            break;
        }
        pthread_mutex_unlock(chunks_list_lock);

        // take an available peer (which server mininum number of chunks)
        pthread_mutex_lock(src_dnodes_list_lock);
        // printf("searching an online peer dnode details..\n");
        int min_dnode_index = -1;
        int min_chunks_served = 1000000;
        for (int i = 0; i < num_peer_nodes; i++) {
            if (src_dnodes_list[i].is_online && 
                src_dnodes_list[i].chunks_served < min_chunks_served) {
                
                if (min_dnode_index < 0) {
                    min_dnode_index = i;
                    min_chunks_served = src_dnodes_list[i].chunks_served;
                    mempcpy(&dnode_struct, &src_dnodes_list[i], sizeof(dnode_struct));
                }
            }
        }
        if (min_dnode_index < 0) { // no online peer is available
            printf("no online peer found..\n");
            pthread_mutex_unlock(src_dnodes_list_lock);
            continue;
        }
        pthread_mutex_unlock(src_dnodes_list_lock);

        // download content from dnode

        // connect to peer
        printf("connecting to peer (port : %d)\n", (int)dnode_struct.port);
        int peer_sockfd = connect_to_server(dnode_struct.ip, dnode_struct.port);
        if (peer_sockfd < 0) {
            printf("unable to connect to peer dnode..\n");

            // update is_downloading of chunk and is_online of dnode
            pthread_mutex_lock(src_dnodes_list_lock);
            pthread_mutex_lock(chunks_list_lock);
            chunks_list[chunk_index].is_downloading = 0;
            src_dnodes_list[min_dnode_index].is_online = 0;
            pthread_mutex_unlock(chunks_list_lock);
            pthread_mutex_unlock(src_dnodes_list_lock);

            continue;
        }

        // fill data request details (download entire data)
        file_data_req_struct file_data_req;
        file_data_req.file_hash = threads_args->file_hash;
        memcpy(file_data_req.file_name, threads_args->file_name, strlen(threads_args->file_name) + 1);
        file_data_req.offset = chunk_struct.offset;
        file_data_req.size = chunk_struct.size;

        // send data request details to peer
        printf("chunk offset : %d\n", file_data_req.offset);
        printf("chunk size : %d\n", file_data_req.size);
        send_full(peer_sockfd, &file_data_req, sizeof(file_data_req));

        // recv chunk data from server
        // uint8_t *buffer = (uint8_t *)malloc(sizeof(chunk_struct.size));

        int buffer_offset = chunk_struct.offset - (current_chunk_index * FILE_CHUNK_SIZE);
        // printf("buffer_offset : %d\n", buffer_offset);
        int bytes_recv = recv_full(peer_sockfd, download_buffer + buffer_offset, chunk_struct.size);
        // int bytes_recv = recv_full(peer_sockfd, buffer, chunk_struct.size);
        std::cout << "bytes recv from peer : " << bytes_recv << std::endl << std::endl;

        // verify checksum
        uint64_t chunk_hash = compute_hash(download_buffer + buffer_offset, chunk_struct.size);
        // uint64_t chunk_hash = compute_hash(buffer, chunk_struct.size);
        printf("chunk checksum (downloaded): %08lx\n", chunk_hash);
        printf("chunk checksum (original) : %08lx\n", chunk_struct.chunk_hash);

        if (chunk_hash = chunk_struct.chunk_hash) {
            printf("verified chunk hash..\n");
        } else {
            printf("unable to verify checksum, received corrupted data\n");
            // TODO
        }
        
        // disconnect from peer dnode
        disconnect_from_server(peer_sockfd);

        // update data structures
        pthread_mutex_lock(chunks_list_lock);
        pthread_mutex_lock(num_chunks_downloaded_lock);
        pthread_mutex_lock(src_dnodes_list_lock);

        chunks_list[chunk_struct.index].is_downloaded = 1;
        src_dnodes_list[min_dnode_index].chunks_served++;

        while(*num_chunks_downloaded_ptr < max_chunk_index) {
            if (chunks_list[*num_chunks_downloaded_ptr].is_downloaded) {
                *num_bytes_downloaded_ptr += chunks_list[*num_chunks_downloaded_ptr].size;
                (*num_chunks_downloaded_ptr) += 1;
            }
            else {
                break;
            }
        }

        pthread_mutex_unlock(src_dnodes_list_lock);
        pthread_mutex_unlock(num_chunks_downloaded_lock);
        pthread_mutex_unlock(chunks_list_lock);

        // break;
    }
}

void multi_threaded_download(struct file_download_res_struct *file_download_res,
                            struct peer_dnode_struct * peer_dnodes_list,
                            uint64_t *chunk_hashes, char *file_name) {

    printf("multi threaded download1..\n");
    // parse file_download_res
    int file_hash = file_download_res->file_hash;
    int file_size = file_download_res->file_size;
    int num_peer_nodes = file_download_res->num_peer_dnodes;

    int complete_chunks = file_size / FILE_CHUNK_SIZE;
    int partial_chunk_size = file_size % FILE_CHUNK_SIZE;
    int num_chunks = complete_chunks;
    if (partial_chunk_size > 0) {
        num_chunks += 1;
    }

    // initialize data structure
    printf("initialization src_dnodes_list..\n");
    struct download_src_dnode_struct *src_dnodes_list;
    int src_dnodes_list_size = num_peer_nodes * sizeof(struct download_src_dnode_struct);
    src_dnodes_list = (struct download_src_dnode_struct *)malloc(src_dnodes_list_size);

    for (int i = 0; i <  num_peer_nodes; i++) {
        // src_dnodes_list[i].ip = peer_dnodes_list[i].ip;
        memcpy(src_dnodes_list[i].ip, peer_dnodes_list[i].ip, strlen(peer_dnodes_list[i].ip) + 1);
        src_dnodes_list[i].port = peer_dnodes_list[i].port;
        src_dnodes_list[i].chunks_served = 0;
        src_dnodes_list[i].is_online = 1; // assume every peer is online initially
    }

    printf("initializing chunks_list..");
    struct download_chunk_struct *chunks_list;
    int chunks_list_size = num_chunks * sizeof(struct download_chunk_struct);
    chunks_list = (struct download_chunk_struct *)malloc(chunks_list_size);

    for (int i = 0; i < num_chunks; i++) {
        chunks_list[i].index = i;
        chunks_list[i].chunk_hash = chunk_hashes[i];
        chunks_list[i].offset = i * FILE_CHUNK_SIZE;
        chunks_list[i].size = FILE_CHUNK_SIZE;
        chunks_list[i].is_downloading = 0;
        chunks_list[i].is_downloaded = 0;
    }
    if (partial_chunk_size > 0) {
        chunks_list[num_chunks - 1].size = partial_chunk_size;
    }

    // download every 10 chunks in one go
    int download_buffer_size = FILE_CHUNK_SIZE * DOWNLOADABLE_CHUNKS;
    uint8_t *download_buffer = (uint8_t *)malloc(download_buffer_size);
    printf("download buffer size : %d\n", download_buffer_size);

    int num_chunks_downloaded = 0;
    int num_bytes_downloaded  = 0;

    pthread_t tid[DOWNLOAD_THREADS];
    pthread_mutex_t num_chunks_downloaded_lock;
    pthread_mutex_t src_dnodes_list_lock;
    pthread_mutex_t chunks_list_lock;
    
    printf("intiializing data structures locks..\n");
    pthread_mutex_init(&num_chunks_downloaded_lock, NULL);
    pthread_mutex_init(&src_dnodes_list_lock, NULL);
    pthread_mutex_init(&chunks_list_lock, NULL);

    // fs handler
    string file_name_str(file_name);
    string out_path = string(dnode_details.files_dir) + string("/") + file_name_str;
    int write_fd = open(out_path.c_str(), O_CREAT | O_WRONLY, 0644);
    int bytes_written = 0;
    
    int it = 0;
    while (num_chunks_downloaded < num_chunks) {

        printf("\ndownload iteration :  %d\n", it);
        printf("num_chunks_downloaded :  %d\n", num_chunks_downloaded);
        printf("num_bytes_downloaded : %d\n", num_bytes_downloaded);

        int current_chunk_index = num_chunks_downloaded;
        int max_chunk_index = min(current_chunk_index + DOWNLOADABLE_CHUNKS, num_chunks);
        struct download_threads_args_struct threads_args;

        threads_args.file_hash = file_hash;
        memcpy(threads_args.file_name, file_name, strlen(file_name) + 1);
        threads_args.download_buffer = download_buffer;
        threads_args.current_chunk_index = current_chunk_index;
        threads_args.max_chunk_index = max_chunk_index;
        threads_args.num_peer_nodes = num_peer_nodes;
        threads_args.num_chunks = num_chunks;

        threads_args.num_chunks_downloaded_ptr = &num_chunks_downloaded;
        threads_args.num_bytes_downloaded_ptr = &num_bytes_downloaded;
        threads_args.src_dnodes_list = src_dnodes_list;
        threads_args.chunks_list = chunks_list;

        threads_args.num_chunks_downloaded_lock = &num_chunks_downloaded_lock;
        threads_args.src_dnodes_list_lock =  &src_dnodes_list_lock;
        threads_args.chunks_list_lock = &chunks_list_lock;

        printf("spawning download threads..\n");
        for (int i = 0; i < DOWNLOAD_THREADS; i++) {
            if (pthread_create(&tid[i], NULL, download_thread_handler, &threads_args) != 0) {
                perror("Failed to create download thread");
            }
        }

        printf("joining download threads..");
        for (int i = 0; i < DOWNLOAD_THREADS; i++) {
            pthread_join(tid[i], NULL);
        }

        // once the chunks are downloaded
        int buffer_end_offset = download_buffer_size;
        if (num_chunks_downloaded == num_chunks) {
            buffer_end_offset = num_bytes_downloaded % download_buffer_size;
        }

        printf("writing download buffer to fs (%d bytes)\n", buffer_end_offset);
        bytes_written += fwrite_full(write_fd, download_buffer, buffer_end_offset);

        break;
    }

    std::cout << "bytes downloaded (net) : " << num_bytes_downloaded << std::endl;
    std::cout << "bytes written (fs) : " << bytes_written << std::endl;
    close(write_fd);
    free(download_buffer);

    printf("destroying locks..\n");
    pthread_mutex_destroy(&num_chunks_downloaded_lock);
    pthread_mutex_destroy(&src_dnodes_list_lock);
    pthread_mutex_destroy(&chunks_list_lock);

}


void naive_download(struct file_download_res_struct *file_download_res,
                    struct peer_dnode_struct * peer_dnodes_list,
                    uint64_t *chunk_hashes, char *file_name) {
    
    // retrieve last peer details
    int num_peer_dnodes = file_download_res->num_peer_dnodes;
    char *peer_ip = peer_dnodes_list[num_peer_dnodes - 1].ip;
    short peer_port = peer_dnodes_list[num_peer_dnodes - 1].port;

    int file_hash = file_download_res->file_hash;
    int file_size = file_download_res->file_size;
    uint8_t* file_data = (uint8_t *)malloc(file_size);
    std::cout << "file size : " << file_size << endl;

    // connect to peer
    std::cout << "connecting to peer dnode.." << std::endl;
    std::cout << "peer port " << peer_port << std::endl;
    int peer_sockfd = connect_to_server(peer_ip, peer_port);

    // fill data request details (download entire data)
    file_data_req_struct file_data_req;
    file_data_req.file_hash = file_hash;
    memcpy(file_data_req.file_name, file_name, strlen(file_name) + 1);
    file_data_req.offset = 0;
    file_data_req.size = file_size;

    // send data request details to peer
    send_full(peer_sockfd, &file_data_req, sizeof(file_data_req));

    // recv chunk data from server
    int bytes_recv= recv_full(peer_sockfd, file_data, file_size);
    std::cout << "bytes recv from peer : " << bytes_recv << std::endl;

    // save file to storage
    string file_name_str(file_name);
    string out_path = string(dnode_details.files_dir) + string("/") + file_name_str;
    int write_fd = open(out_path.c_str(), O_CREAT | O_WRONLY, 0644);
    int bytes_written  = fwrite_full(write_fd, file_data, file_size);
    std::cout << "bytes written (fs) : " << bytes_written << std::endl;
    close(write_fd);

    free(file_data);

    //disconnect from peer
    disconnect_from_server(peer_sockfd);
    return;
}


void handle_file_download(int rpc_cli_fd, char *file_name) {

    // connect to hub
    int hub_sockfd = connect_to_server(dnode_details.hub_ip, dnode_details.hub_port);

    // send download hub command
    struct hub_cmd_struct hub_cmd;
    hub_cmd.cmd_type = FILE_DOWNLOAD;
    send_full(hub_sockfd, &hub_cmd, sizeof(hub_cmd));

    // send download request details
    struct file_download_req_struct file_download_req;
    file_download_req.dnode_uid = dnode_details.uid;
    memcpy(file_download_req.file_name, file_name, strlen(file_name) + 1);
    send_full(hub_sockfd, &file_download_req, sizeof(file_download_req));

    // recv file download response from server
    struct file_download_res_struct file_download_res;
    recv(hub_sockfd, &file_download_res, sizeof(file_download_res), 0);
    int num_peer_dnodes = file_download_res.num_peer_dnodes;
    int file_index_data_size = file_download_res.file_index_data_size;

    printf("file_hash : %016lx\n", file_download_res.file_hash);
    printf("file size : %d \n", file_download_res.file_size);
    printf("num_chunks: %d \n", file_download_res.num_chunks);
    printf("num peer nodes:  %d\n", num_peer_dnodes);

    // intialize file index data and peer dnodes data
    uint8_t *file_index_data = (uint8_t *)malloc(file_index_data_size);
    int peer_dnodes_list_size = sizeof(struct peer_dnode_struct) * num_peer_dnodes;
    struct peer_dnode_struct * peer_dnodes_list;
    peer_dnodes_list = (struct peer_dnode_struct *)malloc(peer_dnodes_list_size);
    
    // recieve file index data
    recv_full(hub_sockfd, file_index_data, file_index_data_size);
    uint64_t* chunk_hashes = (uint64_t *)file_index_data;

    // recv peer nodes list
    recv_full(hub_sockfd, peer_dnodes_list,  peer_dnodes_list_size);

    
    // disconnect from hub
    disconnect_from_server(hub_sockfd);


    /*
    For now, download the entire data from single peer node
    */
    // naive_download(&file_download_res, peer_dnodes_list, chunk_hashes, file_name);
    multi_threaded_download(&file_download_res, peer_dnodes_list, chunk_hashes, file_name);

    // connect to hub
    hub_sockfd = connect_to_server(dnode_details.hub_ip, dnode_details.hub_port);
    
    // send file_downloaded_ack request to hub
    // struct hub_cmd_struct hub_cmd;
    // hub_cmd.cmd_type = FILE_DOWNLOADED_ACK;
    // send_full(hub_sockfd, &hub_cmd, )
    send_cmd_to_hub(hub_sockfd, FILE_DOWNLOADED_ACK);

    // send ack to hub indicating this node is a source of file
    struct file_downloaded_ack_struct file_downloaded_ack;
    file_downloaded_ack.dnode_uid = dnode_details.uid;
    file_downloaded_ack.file_hash = file_download_res.file_hash;
    send_full(hub_sockfd, &file_downloaded_ack, sizeof(file_downloaded_ack));

    // disconnect from server
    disconnect_from_server(hub_sockfd);

    // send success response to rpc client
    struct rpc_res_struct rpc_res;
    rpc_res.res_type = RPC_RES_SUCCEES;
    rpc_res.payload_len = 0;
    send_full(rpc_cli_fd, &rpc_res, sizeof(rpc_res));

    return;
}


// server process 3 (listens for rfc requests)
void *handle_rpc_server(void *args) {

    std::cout << "RPC server :: Starting!!" << std::endl;
    char* rpc_buffer = (char *)malloc(2048); // 2MB all-purpose buffer
    int rpc_port = dnode_details.rpc_port;
    int sockfd = create_server(rpc_port);

    if(sockfd < 0) {
        std::cout << "RPC server :: Unable to create server with port " << rpc_port << std::endl;
        std::cout << "RPC server :: Terminating.. " << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "RPC server :: server created!!" << std::endl;

    if (listen(sockfd, 5) == 0) {
        std::cout << "RPC server :: listening for RPC commands on port " << rpc_port <<std::endl;
    }

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    while(1) {

        int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("Accept error\n");
            std::exit(0);
        }

        std::cout << "\nRPC server :: Handling request" << std::endl;

        struct rpc_req_struct rpc_req;
        recv_full(newsockfd, &rpc_req, sizeof(rpc_req));
        recv_full(newsockfd, rpc_buffer, rpc_req.payload_len);

        if (rpc_req.req_type == RPC_REQ_UPLOAD) {
            std::cout << "RPC server :: upload request " << std::endl;
            char *file_path = rpc_buffer;
            handle_file_upload(newsockfd, file_path);

        } else if (rpc_req.req_type == RPC_REQ_DOWNLOAD) {
            std::cout << "RPC server :: download request " << std::endl;
            char *file_name = rpc_buffer;
            handle_file_download(newsockfd, file_name);

        } else {
            std::cout << "Invalid RPC command " << std::endl;
        }
 
        close(newsockfd);
    }

    close(sockfd);
    std::exit(EXIT_SUCCESS);
}
