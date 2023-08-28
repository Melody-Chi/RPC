#define NONBLOCKING

#include "rpc.h"
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>



typedef struct {
    char* name_function;
    rpc_handler handler_function;
} rpc_function;

struct rpc_server{
    int new_socket_fd;
    struct sockaddr_in address;
    rpc_function* server_function;
    int num_function;
};

typedef struct {
    int newsock_fd;
    rpc_server *srv;
} thread_arg;


int create_listening_socket(char* service);
void *process_request(void *arg);


#ifndef htonll
uint64_t htonll(uint64_t value) {
    int num = 1;
    if(*(char *)&num == 1) {
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}
#endif

#ifndef ntohll
uint64_t ntohll(uint64_t value) {
    int num = 1;
    if(*(char *)&num == 1) {
        uint32_t high_part = ntohl((uint32_t)(value >> 32));
        uint32_t low_part = ntohl((uint32_t)(value & 0xFFFFFFFFLL));
        return (((uint64_t)low_part) << 32) | high_part;
    } else {
        return value;
    }
}
#endif



rpc_server *rpc_init_server(int port) {

    // Code from Practical 9
    int sock_fd;

    int num = snprintf(NULL, 0, "%d", port);
    char *port_str = malloc((num + 1) * sizeof(char));
    if (port_str == NULL) {
        return NULL;
    }
    sprintf(port_str, "%d", port);

    // create listening socket
    sock_fd = create_listening_socket(port_str);

    // Listen on socket - means we're ready to accept connections,
    // incoming connection requests will be queued, man 3 listen
    if (listen(sock_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Initialize the server's fields
    rpc_server *srv = malloc(sizeof(rpc_server));
    assert(srv != NULL);

    srv->new_socket_fd = sock_fd;
    srv->num_function = 0;
    srv->server_function = NULL;

    free(port_str);

    return srv;
}


int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    for (int i=0; i < srv->num_function; i++){
        if (strcmp(srv->server_function[i].name_function, name) == 0) {
            srv->server_function[i].handler_function = handler;
            return 0;
        }
    }

    srv->num_function += 1;
    rpc_function* temp_function =
        realloc(srv->server_function,
                srv->num_function * sizeof(rpc_function));
    if (temp_function == NULL) {
        // free if 'realloc' fails
        free(srv->server_function);
        return -1;
    } else {
        srv->server_function = temp_function;
    }

    if (srv->server_function == NULL) {
        return -1;
    }

    // Successful to register
    srv->server_function[srv->num_function-1].name_function = strdup(name);
    srv->server_function[srv->num_function-1].handler_function = handler;
    return 0;
}

void rpc_serve_all(rpc_server *srv) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_size;

    // Accept a connection - blocks until a connection is ready to be accepted
    // Get back a new file descriptor to communicate on
    client_addr_size = sizeof client_addr;

    while (1) {
        int newsock_fd = accept(srv->new_socket_fd,
                                (struct sockaddr *)&client_addr,
                                &client_addr_size);
        if (newsock_fd < 0) {
            perror("accept");
            continue;
        }
        thread_arg *targ = malloc(sizeof(thread_arg));
        if (targ == NULL) {
            perror("malloc");
            continue;
        }

        targ->newsock_fd = newsock_fd;
        targ->srv = srv;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, process_request, targ) != 0) {
            perror("pthread_create");
            free(targ);
            continue;
        }
        pthread_detach(thread_id);
    }
}


void *process_request(void *arg) {
    thread_arg *targ = (thread_arg *)arg;
    char buffer[1024];
    ssize_t n;

    while (1) {
        char request_type;
        n = read(targ->newsock_fd, &request_type, 1);
        if (n <= 0) {
            break;
        }
        if (request_type == 'N') {
            // Close the connection
            close(targ->newsock_fd);
            pthread_exit(NULL);
        } else if (request_type == 'F') {
            // rpc_find
            n = read(targ->newsock_fd, buffer, sizeof(buffer));

            buffer[n] = '\0';

            char response = 'N';
            for (int i = 0; i < targ->srv->num_function; i++) {
                if (strcmp(targ->srv->server_function[i].name_function,
                           buffer) == 0) {
                    // the function exists
                    response = 'Y';
                    break;
                }
            }

            // Send the response
            write(targ->newsock_fd, &response, 1);

        } else if (request_type == 'C') { // rpc_call
            int name_len;
            // rpc call, read the function name and t data
            // call the function and send back the result
            rpc_data input;
            n = read(targ->newsock_fd, &name_len, sizeof(int));
            if (n <= 0) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            char *function_name = malloc(name_len + 1);

            if (function_name == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }

            n = read(targ->newsock_fd, function_name, name_len);
            if (n <= 0) {
                perror("read");
                free(function_name);
                exit(EXIT_FAILURE);
            }
            function_name[n] = '\0';

            // read data1
            int64_t network_data1;
            n = read(targ->newsock_fd, &network_data1, sizeof(int64_t));
            if (n <= 0) {
                perror("read");
                free(function_name);
                exit(EXIT_FAILURE);
            }
            input.data1 = (int)ntohll(network_data1);

            // read the length of data2
            n = read(targ->newsock_fd, &input.data2_len, sizeof(size_t));
            if (n <= 0) {
                perror("read");
                free(function_name);
                exit(EXIT_FAILURE);
            }

            if (input.data2_len != 0) {
                input.data2 = malloc(input.data2_len);
                if (input.data2 == NULL) {
                    perror("malloc");
                    free(function_name);
                    exit(EXIT_FAILURE);
                }

                // read data2
                n = read(targ->newsock_fd, input.data2, input.data2_len);
                if (n <= 0) {
                    perror("read");
                    free(input.data2);
                    free(function_name);
                    exit(EXIT_FAILURE);
                }
            } else {
                input.data2 = NULL;
            }

            // function lookup and call
            rpc_data *output = NULL;
            int found = 0;
            for (int i = 0; i < targ->srv->num_function; i++) {
                if (strcmp(targ->srv->server_function[i].name_function,
                           function_name) == 0) {
                    found = 1;
                    output =
                        targ->srv->server_function[i].handler_function(&input);
                    break;
                }
            }

            // errors
            if (!found || output == NULL ||
                (output->data1==0 && output->data2==NULL)
                ||(output->data1==0 && output->data2_len==0) ) {

                int64_t error_data1 = htonll(-1);
                write(targ->newsock_fd,
                      &error_data1, sizeof(int64_t));
                free(input.data2);
                continue;
            }
            int64_t data1_network = htonll((int64_t)(output->data1));
            write(targ->newsock_fd, &data1_network,
                  sizeof(int64_t));
            fsync(targ->newsock_fd);
            write(targ->newsock_fd, &output->data2_len,
                  sizeof(size_t));
            fsync(targ->newsock_fd);

            // Write data2 back
            if (output->data2_len > 0 && output->data2 != NULL) {
                write(targ->newsock_fd, output->data2,
                      output->data2_len);
            }
            free(function_name);
            free(input.data2);

        } else {
            close(targ->newsock_fd);
            pthread_exit(NULL);
        }
    }
    pthread_exit(NULL);
}



struct rpc_client {
    int socket_id;
};

rpc_client *rpc_init_client(char *addr, int port) {

    // Code from Practical 9
    int sockfd, s;
    struct addrinfo hints, *servinfo, *rp;

    // Create address
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    int num = snprintf(NULL, 0, "%d", port);
    char *port_str = malloc((num + 1) * sizeof(char));
    if (port_str == NULL) {
        return NULL;
    }
    sprintf(port_str, "%d", port);

    // Get addrinfo of server. From man page:
    // The getaddrinfo() function combines the functionality provided by the
    // gethostbyname(3) and getservbyname(3) functions into a single interface
    s = getaddrinfo(addr, port_str, &hints, &servinfo);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Connect to first valid result
    // Why are there multiple results? see man page (search 'several reasons')
    // How to search? enter /, then text to search for, press n/N to navigate
    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1)
            continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; // success

        close(sockfd);
    }
    if (rp == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return NULL;
    }
    freeaddrinfo(servinfo);

    rpc_client* cl = malloc(sizeof(rpc_client));
    assert(cl != NULL);
    cl->socket_id = sockfd;
    return cl;
}

struct rpc_handle{
    int sockfd;
    char *name;
};

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    if (cl == NULL || name == NULL) {
        return NULL;
    }

    ssize_t num;
    char response;
    rpc_handle *handle;

    char request_type = 'F';
    write(cl->socket_id, &request_type, 1);

    num = write(cl->socket_id, name, strlen(name));
    if (num != strlen(name)) {
        return NULL;
    }

    num = read(cl->socket_id, &response, 1);
    //    printf("After read name find\n");
    if (num <= 0) {
        return NULL;
    }

    if (response == 'Y') {
        // create a handle
        handle = malloc(sizeof(rpc_handle));
        if (handle == NULL) {
            return NULL;
        }
        handle->name = strdup(name);
        if (handle->name == NULL) {
            free(handle);
            return NULL;
        }
        handle->sockfd = cl->socket_id;

        return handle;
    }
    return NULL;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    if (!cl || !h || !payload ||
        ((payload->data2_len == 0) != (payload->data2 == NULL))
        || sizeof(payload->data1) > sizeof(int64_t)) {
        return NULL;
    }

    char request_type = 'C';
    ssize_t num_bytes = write(cl->socket_id, &request_type, 1);
    if (num_bytes != 1) {
        return NULL;
    }

    size_t name_len;
    name_len = strlen(h->name);

    write(cl->socket_id, &name_len, sizeof(int));
    write(cl->socket_id, h->name, strlen(h->name));

    int64_t network_data1 = htonll((int64_t)(payload->data1));
    write(cl->socket_id, &network_data1, sizeof(int64_t));

    if (payload->data2 != NULL) {
        write(cl->socket_id, &payload->data2_len, sizeof(size_t));
        write(cl->socket_id, payload->data2, payload->data2_len);
    } else{
        size_t no_data2 = 0;
        write(cl->socket_id, &no_data2, sizeof(size_t));
    }

    rpc_data *response = malloc(sizeof(rpc_data));
    if(response == NULL){
        return NULL;
    }

    // Read the response
    ssize_t n;
    int64_t data1_network;
    n = read(cl->socket_id, &data1_network, sizeof(int64_t));
    response->data1 = (int)ntohll(data1_network);
    if(n <= 0){
        free(response);
        return NULL;
    }

    n = read(cl->socket_id, &response->data2_len, sizeof(size_t));
    if(n <= 0){
        free(response);
        return NULL;
    }

    if(response->data2_len > 0) {
        response->data2 = malloc(response->data2_len);
        if(response->data2 == NULL){
            free(response);
            return NULL;
        }

        n = read(cl->socket_id, response->data2, response->data2_len);  // Read data2 if data2_len is greater than 0
        if(n <= 0){
            free(response->data2);
            free(response);
            return NULL;
        }
    } else if((response->data2 = NULL)){
        return NULL;
    } else {
        response->data2 = NULL;
    }
    return response;

}

void rpc_close_client(rpc_client *cl) {
    char close_type = 'N';
    write(cl->socket_id, &close_type, sizeof(char));
    close(cl->socket_id);
    free(cl);
}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}


int create_listening_socket(char* service) {

    // Code from Practical 9
    int re, s, sockfd;
    struct addrinfo hints, *res;

    // Create address we're going to listen on (with given port number)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;       // IPv6
    hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
    // node (NULL means any interface), service (port), hints, res
    s = getaddrinfo(NULL, service, &hints, &res);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Reuse port if possible
    re = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    // Bind address to the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);
    return sockfd;
}


