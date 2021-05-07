/*
** serverA.cpp -- a datagram sockets "server"
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <iomanip>

using namespace std;

#define SERVER_A_PORT "21623"

#define MAX 4096

const int maxn = 1000;

const int INF = 1e9;

class Edge {
public:
    int start;
    int end;
    int distance;
    Edge(int start, int end, int distance) {
        this->start = start;
        this->end = end;
        this->distance = distance;
    }
};

// vertex does not have to be consecutive and start from 0.  eg: [2, 5, 7, 9]
class Graph {
public:
    string vp;
    string vt;
    set<int> vertices;
    vector<Edge> edges;
    Graph() {}

    void addEdge(int start, int end, int distance) {
        vertices.insert(start);
        vertices.insert(end);
        Edge order(start, end, distance);
        edges.push_back(order);
        Edge reverse(end, start, distance);
        edges.push_back(reverse);
    }
};

struct Request {  // the struct Request serverA will receive from aws
    char map_id[100];
    char src_index[100];
    char file_size[100];
};

struct Reply{  // the struct Reply serverA will send to aws
    char vp[100];
    char vt[100];
    char vertices[100];
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

int main(void) {
    /////////////////////////////////////////////////////////
    ////Boot up and use UDP to connect with aws//////////////
    /////////////////////////////////////////////////////////
    int sockfd;
    int rv;
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    // get address info of serverA
    if ((rv = getaddrinfo(NULL, SERVER_A_PORT, &hints, &servinfo)) !=
        0) { // get serverA's own address info as UDP server
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // create UDP socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (::bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    cout << "The Server A is up and running using UDP on port <21623>." << endl;


    while(1) {
        /////////////////////////////////////////////////////
        //receive data from AWS -> buf array-> struct query//
        /////////////////////////////////////////////////////
        char buf[MAX];
        struct sockaddr_storage AWS_addr;
        socklen_t addr_len;
        addr_len = sizeof AWS_addr;
        if (recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &AWS_addr, &addr_len) == -1) {
            perror("recvfrom");
            exit(1);
        }
        struct Request request;
        memset(&request, 0, sizeof(request));
        memcpy(&request, buf, sizeof(request));

        ////////////////////////////////////////////////
        //read data from map.txt and map construction///
        ////////////////////////////////////////////////
        ifstream in("./map.txt");
        string filename, line, temp;
        vector<string> txt;
        if (in) {
            // string line will not include '\n'
            while (getline(in, line)) {
                stringstream input(line);
                while (input >> temp) {
                    txt.push_back(temp);
                }
            }
        } else {
            cout << "no such file" << endl;
        }

        // txt[i] -> all the data and letter in string format seperated by white space and newlines
        int i = 0, digit;
        map<string, Graph> mapData;
        while (i < txt.size()) {
            Graph *graph = new Graph();
            string key = txt[i++];
            graph->vp = txt[i++];
            graph->vt = txt[i++];
            while (1) {
                int start = atoi(txt[i++].c_str());
                int end = atoi(txt[i++].c_str());
                int distance = atoi(txt[i++].c_str());
                graph->addEdge(start, end, distance);
                stringstream input(txt[i]);
                if (!(input >> digit)) {
                    break;
                }
            }

            mapData[key] = *graph;
        }

        cout << "The Server A has constructed a list of <" << mapData.size() << "> maps: " << endl;
        cout << "---------------------------------------------------" << endl;
        cout << "Map ID            Num Vertices         Num Edges" << endl;
        for (map<string, Graph>::iterator it = mapData.begin(); it != mapData.end(); it++) {
            cout << it->first << "                    " << it->second.vertices.size() << "                  "
                 << it->second.edges.size() / 2 << endl;
        }
        cout << "---------------------------------------------------" << endl;

        /////////////////////////////////////////////////////////
        ///////Path finding after receiving the input request////
        /////////////////////////////////////////////////////////
        string map_id = string(request.map_id);
        int vertex = atoi(request.src_index);
        cout << "The Server A has received input for finding shortest paths: starting vertex <" << vertex
             << "> of map <" << map_id << ">." << endl;


        //////////////////////////////////////////////////////////////
        ///////Finish Dijkstra to find shortest length////////////////
        //////////////////////////////////////////////////////////////
        int minDist[maxn];//最短路
        int visited[maxn] = {false};//标记顶点v是否已被访问
        vector<Edge> adjacent = mapData[map_id].edges;//邻接矩阵表示
        set<int> vertices = mapData[map_id].vertices;
        fill(minDist, minDist + maxn, INF);//最短路径初始化为无穷
        minDist[vertex] = 0;

        for (set<int>::iterator it = vertices.begin(); it != vertices.end(); it++) {//循环n次
            int u = -1, min = INF;
            for (set<int>::iterator it = vertices.begin(); it != vertices.end(); it++) {//寻找还未被访问顶点的最短路
                int j = *it;
                if (minDist[j] < min && !visited[j]) {
                    u = j;
                    min = minDist[u];
                }
            }
            if (u == -1) return -1;//如果没找到，则return
            visited[u] = true;//如果找到，则置u被访问

            for (int j = 0; j < adjacent.size(); j++) {//从u出发能够到达的所有顶点
                if (adjacent[j].start != u) {
                    continue;
                }
                int end = adjacent[j].end;
                if (!visited[end]) {
                    if (minDist[u] + adjacent[j].distance < minDist[end]) {//v未被访问&&以u为中介点能够使得dis[v]更短
                        minDist[end] = minDist[u] + adjacent[j].distance;//更新路径
                    }
                }
            }
        }
        cout << "The Server A has identified the following shortest paths: " << endl;
        cout << "---------------------------------------------------" << endl;
        cout << "Destination                              Min Length" << endl;
        cout << "---------------------------------------------------" << endl;
        struct Reply reply;
        strcpy(reply.vp, mapData[map_id].vp.c_str());
        strcpy(reply.vt, mapData[map_id].vt.c_str());
        string index;
        string dist;
        for (set<int>::iterator it = vertices.begin(); it != vertices.end(); it++) {//输出以start为源点，到达各大顶点的最短距离
            index = index + " " + to_string((*it));
            dist = dist + " " + to_string(minDist[*it]);
            cout << *it << "                                         " << minDist[*it] << endl;
        }
        strcpy(reply.vertices, index.c_str());
        strcpy(reply.minDist, dist.c_str());
        cout << "---------------------------------------------------" << endl;

        //////////////////////////////////////////////////////////////
        ///////Sending reply to AWS///////////////////////////////////
        //////////////////////////////////////////////////////////////
        memset(buf, 0, sizeof(buf));
        memcpy(buf, &reply, sizeof(reply));
        if (sendto(sockfd, buf, sizeof(buf), 0,
                   (struct sockaddr *) &AWS_addr, addr_len) == -1) {
            perror("talker: sendto");
            exit(1);
        }
        cout << "The Server A has sent shortest paths to AWS." << endl;
    }

    close(sockfd);
    return 0;
}
