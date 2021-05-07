/*
** server.cpp -- a stream socket server
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

using namespace std;

#define HOST_NAME "localhost" // the serverA/serverB's hostname

#define AWS_TCP_PORT "24623"

#define AWS_UDP_PORT "23623"

#define SERVER_A_PORT "21623"

#define SERVER_B_PORT "22623"

#define BACKLOG 10   // how many pending connections queue will hold

#define MAX 4096

struct Request {  // the struct Request aws will receive from client and send to serverA
    char map_id[100];
    char start_vertex[100];
    char file_size[100];
};

struct Reply{  // the struct Reply aws will receive from serverA
    char vp[100];
    char vt[100];
    char vertices[100];
    char minDist[100];
};

struct Query { // the struct Query aws will send to serverB
    char file_size[100];
    char vp[100];
    char vt[100];
    char vertices[100];
    char minDist[100];
};

struct Response { // the struct Response aws will receive from serverB and send to client
    double tp[100];
    double tt[100];
    char vertices[100];
    double delay[100];
    char minDist[100];
};

void sigchld_handler(int s)
{
    (void)s; // quiet unused variable warning
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
    /////////////////////////////////////////////////////////
    ////Boot up and use  TCP socket to connect with client///
    /////////////////////////////////////////////////////////
    // listen on sock_fd, communicate on new_fd
    int sockfd, new_fd;
    // get aws's own address info (port number and ip address)
    int rv;
    int yes = 1;
    struct addrinfo hints, *tcp_aws_info, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, AWS_TCP_PORT, &hints, &tcp_aws_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results, create socket and bind local address
    for(p = tcp_aws_info; p != NULL; p = p->ai_next) {
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
        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    freeaddrinfo(tcp_aws_info); // all done with this structure

    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    cout << "The AWS is up and running." << endl;


    ///////////////////////////////////////////////////////////
    //Get address info of aws and specify its UDP port number//
    ///////////////////////////////////////////////////////////
    int udp_socket;
    struct addrinfo *udp_aws_info, *q;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, AWS_UDP_PORT, &hints, &udp_aws_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    /////////////////////////////////////////////////////////
    //Get address info of server A(used to specify the destination)
    /////////////////////////////////////////////////////////
    struct addrinfo *serverAinfo, *r;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(HOST_NAME, SERVER_A_PORT, &hints, &serverAinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and get address info of server A
    for(r = serverAinfo; r != NULL; r = r->ai_next) {
        if (r != NULL) {
            break;
        }
    }
    if (r == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    // freeaddrinfo(serverAinfo);

    /////////////////////////////////////////////////////////
    //Get address info of server B(used to specify the destination)
    /////////////////////////////////////////////////////////
    struct addrinfo *serverBinfo, *s;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(HOST_NAME, SERVER_B_PORT, &hints, &serverBinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and make a UDP socket connecting to server B
    for(s = serverBinfo; s != NULL; s = s->ai_next) {
        if (s != NULL) {
            break;
        }
    }
    if (s == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    // freeaddrinfo(serverBinfo);


    /////////////////////////////////////////////////////////
    ////receiving request from the client////////////////////
    /////////////////////////////////////////////////////////
    // receive data from buf and convert buf array into struct request
    char buf[MAX];
    memset(buf,0, sizeof(buf));
    struct Request request;
    socklen_t addr_len;
    struct sockaddr_storage client_addr; // connector---client's address information
    while(1) {
        addr_len = sizeof client_addr; // create child socket to communicate with aws
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        if (!fork()) {  // create child process to receive query
            if (recv(new_fd, buf, sizeof buf, 0) == -1) {
                perror("recv");
                exit(1);
            }
            // data -> buf array -> struct request
            memset(&request,0,sizeof(request));
            memcpy(&request, buf, sizeof(request));

            cout << "The AWS has received map ID <" << request.map_id << ">, star vertex <" << request.start_vertex << "> and file size <" << request.file_size << "> from the client using TCP over port <24623>" << endl;

            /////////////////////////////////////////////////////////
            /////create UDP socket with server A/////////////////////
            /////////////////////////////////////////////////////////
            // loop through all the results and bind UDP port number to UDP socket
            for(q = udp_aws_info; q != NULL; q = q->ai_next) {
                if ((udp_socket = socket(q->ai_family, q->ai_socktype,
                                         q->ai_protocol)) == -1) {
                    perror("listener: socket");
                    continue;
                }
                if (::bind(udp_socket, q->ai_addr, q->ai_addrlen) == -1) {
                    close(udp_socket);
                    perror("listener: bind");
                    continue;
                }

                break;
            }
            if (q == NULL) {
                fprintf(stderr, "listener: failed to bind socket\n");
                return 2;
            }
            // freeaddrinfo(udp_aws_info);

            /////////////////////////////////////////////////////////
            ////aws use UDP to send request to server A//////////////
            /////////////////////////////////////////////////////////
            if (sendto(udp_socket, buf, sizeof(buf), 0,
                       r->ai_addr, r->ai_addrlen) == -1) {
                perror("talker: sendto");
                exit(1);
            }
            cout << "The AWS has sent map ID and starting vertex to server A using UDP over port <23623>." << endl;


            /////////////////////////////////////////////////////////
            ////aws use UDP to receive reply from server A///////////
            /////////////////////////////////////////////////////////
            memset(buf, 0, sizeof(buf));
            if (recvfrom(udp_socket, buf, sizeof(buf), 0, r->ai_addr, &(r->ai_addrlen)) == -1) {
                perror("recvfrom");
                exit(1);
            }
            // close the UDP socket between aws and server A
            close(udp_socket);


            struct Reply reply;
            memset(&reply,0,sizeof(reply));
            memcpy(&reply, buf, sizeof(reply));
            cout << "The AWS has received shortest path from server A: " << endl;
            cout << "--------------------------------------------" << endl;
            cout << "Destination        Min Length" << endl;
            cout << "--------------------------------------------" << endl;

            istringstream minDist(string(reply.minDist));
            istringstream vertices(string(reply.vertices));
            string out1, out2;
            while (minDist >> out2 && vertices >> out1) {
                cout << out1 << "                   " << out2 << endl;
            }
            cout << "--------------------------------------------" << endl;


            /////////////////////////////////////////////////////////
            ////create UDP socket and  communicate with server B/////
            /////////////////////////////////////////////////////////
            // loop through all the results and bind UDP port number to UDP socket
            for(q = udp_aws_info; q != NULL; q = q->ai_next) {
                if ((udp_socket = socket(q->ai_family, q->ai_socktype,
                                         q->ai_protocol)) == -1) {
                    perror("listener: socket");
                    continue;
                }
                if (::bind(udp_socket, q->ai_addr, q->ai_addrlen) == -1) {
                    close(udp_socket);
                    perror("listener: bind");
                    continue;
                }

                break;
            }
            if (q == NULL) {
                fprintf(stderr, "listener: failed to bind socket\n");
                return 2;
            }
            // freeaddrinfo(udp_aws_info);

            /////////////////////////////////////////////////////////
            ////sending query to server B////////////////////////////
            /////////////////////////////////////////////////////////
            struct Query query;
            strcpy(query.vp, reply.vp);
            strcpy(query.vt, reply.vt);
            strcpy(query.vertices, reply.vertices);
            strcpy(query.file_size, request.file_size);
            strcpy(query.minDist, reply.minDist);
            memset(buf,0, sizeof(buf));
            memcpy(buf, &query, sizeof(query));
            if (sendto(udp_socket, buf, sizeof(buf), 0,
                       s->ai_addr, s->ai_addrlen) == -1) {
                perror("talker: sendto");
                exit(1);
            }
            cout << "The AWS has sent path length, propagation speed and transmission speed to server B using UDP over port <23623>." << endl;

            /////////////////////////////////////////////////////////
            ////receiving response from server B/////////////////////
            /////////////////////////////////////////////////////////
            struct Response response;
            memset(&buf,0, sizeof(buf));
            if (recvfrom(udp_socket, buf, sizeof(buf), 0, s->ai_addr, &(s->ai_addrlen)) == -1) {
                perror("recvfrom");
                exit(1);
            }
            // close the UDP socket between aws and server B
            close(udp_socket);

            memset(&response,0,sizeof(response));
            memcpy(&response, buf, sizeof(response));
            cout << "The AWS has received delays from server B: " << endl;
            cout << "--------------------------------------------" << endl;
            cout << "Destination        Tt          Tp      Delay" << endl;
            cout << "--------------------------------------------" << endl;

            istringstream index(string(response.vertices));
            string out;
            int i = 0;
            while (index >> out) {
                cout << out << "                 " << fixed << setprecision(2) << response.tt[i] << "        " << response.tp[i] << "      " << response.delay[i] << endl;
                i++;
            }
            cout << "--------------------------------------------" << endl;

            /////////////////////////////////////////////////////////
            ////sending response to client///////////////////////////
            /////////////////////////////////////////////////////////
            memset(buf,0, sizeof(buf));
            strcpy(response.minDist, reply.minDist);
            memcpy(buf, &response, sizeof(response));
            if (send(new_fd, buf, sizeof(buf), 0) == -1) {
                perror("send");
            }
            cout << "The AWS has sent calculated delay to client using client TCP over port <24623>." << endl;

            close(new_fd);
            exit(0);
        }
    }
    close(sockfd);
    return 0;
}
