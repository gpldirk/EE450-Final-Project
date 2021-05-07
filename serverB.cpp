/*
** serverB.c -- a datagram "client"
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

using namespace std;

#define SERVER_B_PORT "22623"

#define MAX 4096

struct Query { // the struct Query aws will send to serverB
    char file_size[100];
    char vp[100];
    char vt[100];
    char vertices[100];
    char minDist[100];
};

struct Response { // the struct Response serverA will send to aws
    double tp[100];
    double tt[100];
    char vertices[100];
    double delay[100];
    char minDist[100];
};

int main(int argc, char *argv[])
{
    /////////////////////////////////////////////////////////
    ////Boot up and use UDP to connecte with aws/////////////
    /////////////////////////////////////////////////////////
    int sockfd;
    int rv;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, SERVER_B_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and make a UDP socket connected with aws
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    cout << "The Server B is up and running using UDP on port <22623>." << endl;


    while(1) {
        ///////////////////////////////////////////////
        //Receiving query from AWS and do calculation//
        ///////////////////////////////////////////////
        char buf[MAX];
        struct Query query;
        struct sockaddr_storage AWS_addr;
        socklen_t addr_len;
        addr_len = sizeof AWS_addr;
        if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &AWS_addr, &addr_len) == -1) {
            perror("recvfrom");
            exit(1);
        }
        memset(&query, 0, sizeof(query));
        memcpy(&query, buf, sizeof(query));

        cout << "The Server B has received data for calculation: " << endl;
        cout << "* Propagation speed: <" << query.vp << "> km/s;" << endl;
        cout << "* Transmission speed: <" << query.vt << "> Bytes/s;" << endl;

        struct Response response;
        strcpy(response.vertices, query.vertices);
        istringstream minDist(string(query.minDist));
        istringstream vertices(string(query.vertices));
        string out1, out2;
        int i = 0;
        while (minDist >> out1 && vertices >> out2) {
            response.tp[i] = atof(out1.c_str()) / atof(query.vp);
            response.tt[i] = atof(query.file_size) / atof(query.vt);
            response.delay[i] = response.tp[i] + response.tt[i];
            i++;
            cout << "* Path length for destination <" << out2 << ">: <" << out1 << ">;" << endl;
        }

        cout << "The Server B has finished the calculation of the delays: " << endl;
        cout << "-----------------------------" << endl;
        cout << "Destination             Delay" << endl;
        cout << "-----------------------------" << endl;
        i = 0;
        string out3;
        istringstream index(string(query.vertices));
        while (index >> out3) {
            cout << out3 << "                       " << fixed << setprecision(2) << response.delay[i] << endl;
            i++;
        }
        cout << "-----------------------------" << endl;

        ///////////////////////////////////////////////
        /////Sending response to AWS///////////////////
        ///////////////////////////////////////////////
        memset(buf, 0, sizeof(buf));
        memcpy(buf, &response, sizeof(response));
        if (sendto(sockfd, buf, sizeof(buf), 0,
                   (struct sockaddr *) &AWS_addr, addr_len) == -1) {
            perror("talker: sendto");
            exit(1);
        }
        cout << "The Server B has finished sending the output to AWS." << endl;
    }

    close(sockfd);
    return 0;
}
