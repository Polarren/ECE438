#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <set>
#include <vector>

using namespace std;

struct message
{
    int src, dest;
    string msg;

};

vector<message> messageVec;

unordered_map<int,unordered_map<int, int>> topo;

set<int> nodes;

unordered_map<int, unordered_map<int, pair<int, int> > > forward_table;

ofstream fpOut;


void initForwardTable() {
    int src, dest;
    for (auto i = nodes.begin(); i != nodes.end(); ++i) {
        src = *i;
        for (auto j = nodes.begin(); j != nodes.end(); ++j) {
            dest = *j;
            if (src == dest) {
                topo[src][dest] = 0;
            } else {
                if (topo[src].count(dest)==0){
                    topo[src][dest]=-999;
                }
            }
            forward_table[src][dest] = make_pair(dest, topo[src][dest]);
        }
    }
}


void getTopo(string topofile){
    ifstream topoin;

    topoin.open(topofile, ios::in);
    if (!topoin) {
        cout << "open file error!" << endl;
    }
    //get topology
    int src, dest, cost;
    while (topoin >> src >> dest >> cost) {
        if (nodes.count(src)<1) {
            nodes.insert(src);
        }
        if (nodes.count(dest)<1) {
            nodes.insert(dest);
        }
        topo[src][dest] = cost;
        topo[dest][src] = cost;
        
    }

    //get first fowarding table
    initForwardTable();
    topoin.close();
}

void updateForwardTable() {
    int src, dest, min, minCost;
    for (auto i =nodes.begin();i!=nodes.end();++i){
        for (auto j = nodes.begin(); j != nodes.end(); ++j) {
            src = *j;
            for (auto k = nodes.begin(); k != nodes.end(); ++k) {
                dest = *k;
                min = forward_table[src][dest].first;
                minCost = forward_table[src][dest].second;
                for (auto m = nodes.begin(); m != nodes.end(); ++m) {
                    if (topo[src][*m] >= 0 && forward_table[*m][dest].second >= 0) {
                        int tempCost = topo[src][*m] + forward_table[*m][dest].second;
                        if (minCost < 0 || tempCost < minCost) {
                            min = *m;
                            minCost = tempCost;
                        }
                    }
                }
                forward_table[src][dest] = make_pair(min, minCost);
            }
        }
    }
}

void printFowardTable(){
    fpOut << endl;
    int src, dest;
    for (auto i = nodes.begin(); i != nodes.end(); ++i) {
        src = *i;
        for (auto j = nodes.begin(); j != nodes.end(); ++j) {
            dest = *j;
            fpOut << dest << " " << forward_table[src][dest].first << " " << forward_table[src][dest].second << endl;
        }
    }
}


void getMessage(string message_file){
    ifstream msgfile(message_file);

    string line, msg;
    int src, dest;
    if (msgfile.is_open()) {
        while (getline(msgfile, line)) {
            
            sscanf(line.c_str(), "%d %d %*s", &src, &dest);
            msg = line.substr(line.find(" "));
            msg = msg.substr(line.find(" ") + 1);
            message newMessage;
            newMessage.src=src;
            newMessage.dest=dest;
            newMessage.msg = msg;
            messageVec.push_back(newMessage);
        }
    }
    msgfile.close();
}


void sendMessage() {
    int src, dest, cost, nexthop;
    for (int i = 0; i < messageVec.size(); ++i) {
        src = messageVec[i].src;
        dest = messageVec[i].dest;
        nexthop = src;
        fpOut << "from " << src << " to " << dest << " cost ";
        cost = forward_table[src][dest].second;
        if (cost == -999) {
            fpOut << "infinite hops unreachable ";
        } else {
            fpOut << cost << " hops ";
            while (nexthop != dest) {
                fpOut << nexthop << " ";
                nexthop = forward_table[nexthop][dest].first;
            }
        }
        fpOut << "message " << messageVec[i].msg << endl;
    }
}

void doChanges(string changefile){
    ifstream changein;

    changein.open(changefile, ios::in);
    if (!changein) {
        cout << "open file error!" << endl;
    }
    //get topology
    int src, dest, cost;
    while (changein >> src >> dest >> cost) {
        if (nodes.count(src)<1) {
            nodes.insert(src);
        }
        if (nodes.count(dest)<1) {
            nodes.insert(dest);
        }
        topo[src][dest] = cost;
        topo[dest][src] = cost;

        initForwardTable();
        updateForwardTable();
        printFowardTable();
        sendMessage();
    }

    
    changein.close();
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }
    string topofile, messagefile, changesfile;
    topofile = argv[1];
    messagefile = argv[2];
    changesfile = argv[3];

    getTopo(topofile);

    fpOut.open("output.txt");
    updateForwardTable();
    printFowardTable();
    getMessage(messagefile);
    sendMessage();
    doChanges(changesfile);

    fpOut.close();
    return 0;
}