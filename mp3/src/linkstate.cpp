#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<fstream>
#include<string>
#include<sstream>
#include<vector>
#include<limits>
#include<algorithm>
#include<unordered_map>
#include<set>
using namespace std;

#define INFINITE 2147483647
// to access: [node_a's id][node_b's id] and returns the cost
ofstream outFile ("output.txt");
typedef struct message_data_t{
    int src;
    int dst;
    string data;
    message_data_t(int _src, int _dst, string _data){
        src = _src;
        dst = _dst;
        data = _data;
    }
} message_data_t;

typedef struct routing_data_t{
    int dst;    // destination from curr node
    int nxt_hop; // nexthop to reach the dst node
    int cost; // path cost
    routing_data_t(int _dst = 1234569, int _nxt_hop = 1234569, int _cost = 1234569){
        dst = _dst;
        nxt_hop = _nxt_hop;
        cost = _cost;
    }
} routing_data_t;

unordered_map<int, unordered_map<int, int> > topoGraph;
set<int> all_nodes;

vector<message_data_t> msg_list;
unordered_map< int, unordered_map<int, routing_data_t> > node_routings;


unordered_map<int, routing_data_t> dijkstra(int src){
    unordered_map<int, routing_data_t> forward_table;
    vector<int> N;
    unordered_map<int, int> path_dict; // the nodes in a map, do backtrace to find actual path. to be used for nexthop
    unordered_map<int, int> D;

    // initialization for Dijkstra
    N.push_back(src);
    for(auto v : all_nodes){
        if(topoGraph[src].count(v) > 0){
            D[v] = topoGraph[src][v];
        } else {
            D[v] = INFINITE;
        }
        path_dict[v] = src;
    }

    // START LOOP
    while( N.size() != all_nodes.size() ){
        int min_node_id = INFINITE;
        int min_distance = INFINITE;

        // find w not in N' such that D(w) is a minimum
        for( auto w : all_nodes ){
            if(find(N.begin(), N.end(), w) == N.end()){
                if(D[w] < min_distance){
                    min_distance = D[w];
                    min_node_id = w;
                }
                if(D[w] == min_distance){
                    // tie break
                    min_node_id = min(min_node_id, w);
                }
            }
        }

        // add min_node to N'
        N.push_back(min_node_id);
        
        // update D(v) for all v adjacent to w and not in N' :
        // D(v) = min( D(v), D(w) + c(w,v) )
        // find(N.begin(), N.end(), min_neighbor_cost.first)
        for(auto min_neighbor_cost : topoGraph[min_node_id]){
            if( find(N.begin(), N.end(), min_neighbor_cost.first) == N.end() ){
                // avoid overflow
                if(D[min_node_id] != INFINITE){
                    int prev_min_neighbor_cost = D[min_neighbor_cost.first];
                    int new_min_neighbor_cost = D[min_node_id] + min_neighbor_cost.second;
                    D[min_neighbor_cost.first] = min(prev_min_neighbor_cost, new_min_neighbor_cost);

                    // check for tie-breaking
                    if(prev_min_neighbor_cost == new_min_neighbor_cost){
                        path_dict[min_neighbor_cost.first] = min(min_node_id, path_dict[min_neighbor_cost.first]);
                    } else if( new_min_neighbor_cost < prev_min_neighbor_cost){
                        path_dict[min_neighbor_cost.first] = min_node_id;
                    }
                }
            }
        }
    }

    // now make the forward table
    forward_table.insert(make_pair(src, routing_data_t(src, src, 0)));
    //forward_table[src] = routing_data_t(src, src, 0);
    for(auto f_node : N){
        if(path_dict[f_node] == src){
            forward_table.insert(make_pair(f_node, routing_data_t(f_node, f_node, D[f_node])));
           //  forward_table[f_node] = routing_data_t(f_node, f_node, D[f_node]);
        } else {
            //int next = forward_table[path_dict[f_node]].nxt_hop;
            forward_table.insert(make_pair(f_node, routing_data_t(f_node, (int)forward_table[(int)path_dict[f_node]].nxt_hop, D[f_node]) ));
            // forward_table[f_node] = routing_data_t(f_node, forward_table[path_dict[f_node]].nxt_hop, D[f_node]);
        }
    }
    return forward_table;
}

void parse_topology(string filepath){
    ifstream myFile(filepath);
    string line;
    int node_a, node_b, cost;
    while(myFile >> node_a >> node_b >> cost){
        all_nodes.insert(node_a);
        all_nodes.insert(node_b);
        topoGraph[node_a][node_b] = cost;
        topoGraph[node_b][node_a] = cost;
    }
    myFile.close();
    return;
}

void parse_messages(string filepath){
    ifstream myFile(filepath);
    string line, msg_data;
    int src, dst;
    while(getline(myFile, line)){
        if(line == ""){continue; }
        stringstream temp_ss(line);
        temp_ss >> src;
        temp_ss >> dst;
        getline(temp_ss, msg_data);
        message_data_t message(src, dst, msg_data.substr(1));
        msg_list.push_back(message);
    }
}

void write_table(){


    int max_node = *(all_nodes.rbegin());
    for(int i = 1; i<= max_node; i++){
        for (int j = 1; j <= max_node; j++){
            int destination = node_routings[i][(int)j].dst;
            int next_hop = node_routings[i][(int)j].nxt_hop;
            int cost = node_routings[i][(int)j].cost;
            if(cost != INFINITE){
                outFile << destination << " " << next_hop << " " << cost << "\n";
            }
           // printf("%d %d %d \n", destination, next_hop, cost);
        }
    }
}

void create_forwarding_table(){
    for(int node : all_nodes){
        unordered_map<int, routing_data_t> temp_forward = dijkstra(node);
        // write to file here
        node_routings[node] = temp_forward;
    }
    return;
}

void output_path(int src, int dst){
    // treat src as current node it is at
    while(src != dst){
        outFile << src << " ";
        src = node_routings[src][dst].nxt_hop;
    }
}

void send_messages(){
    for(auto message : msg_list){
        int src = message.src;
        int dst = message.dst;
        string data = message.data;

        if(node_routings[src][dst].cost != INFINITE){
            outFile << "from " << src << " to " << dst << " cost " << node_routings[src][dst].cost << " hops ";
            output_path(src, dst);
            outFile <<"message " << data << "\n";
        } else {
            outFile << "from " << src << " to " << dst << " cost infinite hops unreachable message " << data << "\n";
        }
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }

    string topology_fpath = argv[1];
    string msg_fpath = argv[2];
    string changes_fpath = argv[3];

    //FILE *fpOut;
   // fpOut = fopen("output.txt", "w");
    // PARSE TOPOLOGY, PUT IN A MAP
    parse_topology(topology_fpath);

    // PARSE MESSAGES
    parse_messages(msg_fpath);
    // make struct, place in a list:
    //    struct: src_node, dst_node, msg

    // iinitialize forwarding_table
    create_forwarding_table();
    // build it using dijkstra without changes

    // write out the table
    write_table();

    // send msgs according to table
    send_messages();


    // do changes one by one in a loop:
        // in each loop:
            // update cost
            // init forward table again according to changes
            // build the forwarding table using dijkstra
            // send messages
    ifstream changeFile(changes_fpath);
    int node1, node2, change;
    while(changeFile >> node1 >> node2 >> change){
        if(change == -999){
            topoGraph[node1].erase(node2);
            topoGraph[node2].erase(node1);
        } else {
            topoGraph[node1][node2] = change;
            topoGraph[node2][node1] = change;
        }
        create_forwarding_table();
        write_table();
        send_messages();
    }

    // fclose(fpOut);
    

    return 0;
}

