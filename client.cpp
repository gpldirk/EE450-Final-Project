/*
** client.cpp -- a stream socket client
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

#define AWS_NAME "localhost" // the aws server hostname client will be connecting to

#define AWS_TCP_PORT "24623" // the aws server port number client will be connecting to

#define MAX 4096 // buf array size

struct Request {  // the struct Request client will send to aws
    char map_id[100];
    char start_vertex[100];
    char file_size[100];
};

struct Response {  // the struct Response client will receive from aws
    double tp[100];
    double tt[100];
    char vertices[100];
    double delay[100];
    char minDist[100];
};

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
    /////////////////////////////////////////////////////////
    ////Boot up and use TCP socket to connect with aws server
    /////////////////////////////////////////////////////////
    // get map_id, start_index and file_size from terminal
    if (argc != 4) {
        fprintf(stderr, "usage: client <Map ID> <Source Vertex Index> <File Size>\n");
        exit(1);
    }
    // put 3 parameters into struct request -> buf array -> ready to send
    char buf[MAX];
    struct Request request;
    strcpy(request.map_id, argv[1]);
    strcpy(request.start_vertex, argv[2]);
    strcpy(request.file_size, argv[3]);
    memset(buf,0, sizeof(buf));
    memcpy(buf, &request, sizeof(request));

    // get aws server ip address by its hostname and specified port number
    int rv;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((rv = getaddrinfo(AWS_NAME, AWS_TCP_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and create tcp socket to connect with aws server
    int sockfd;
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }

        break;
    }
    freeaddrinfo(servinfo);
    cout << "The client is up and running." << endl;


    /////////////////////////////////////////////////////////
    ////Using TCP socket to send query to AWS////////////////
    /////////////////////////////////////////////////////////
    // send buf data to aws by TCP socket
    if (send(sockfd, buf, sizeof(buf), 0) == -1) {
        perror("sent");
        exit(1);
    }

    // get client ip address and port number by socket name
    struct sockaddr_in client;
    socklen_t cLen = sizeof(client);
    getsockname(sockfd, (struct sockaddr*) &client, &cLen);
    cout << "The client has sent query to AWS using TCP over port <" << ntohs(client.sin_port) << ">: start vertex <" << request.start_vertex << ">; map <" << request.map_id << ">; file size <" << request.file_size << ">." << endl;


    /////////////////////////////////////////////////////////
    ////Receiving response from AWS//////////////////////////
    /////////////////////////////////////////////////////////
    // receive buf data into struct response
    memset(buf,0, sizeof(buf));
    if (recv(sockfd, buf, sizeof(buf), 0) == -1) {
        perror("recv");
        exit(1);
    }
    struct Response response;
    memset(&response, 0, sizeof(response));
    memcpy(&response, buf, sizeof(response));

    cout << "The client has received results from AWS: " << endl;
    cout << "---------------------------------------------------------------" << endl;
    cout << "Destination    Min Length        Tt           Tp         Delay" << endl;
    cout << "---------------------------------------------------------------" << endl;
    istringstream vertices(string(response.vertices));
    istringstream minDist(string(response.minDist));
    string out1, out2;
    int i = 0;
    while (vertices >> out1 && minDist >> out2) {
        cout << out1 << "               " << out2 << fixed << setprecision(2) << "               "<< response.tt[i] << "         " << response.tp[i] << "       " << response.delay[i] << endl;
        i++;
    }
    cout << "---------------------------------------------------------------" << endl;

    close(sockfd);
    return 0;
}
