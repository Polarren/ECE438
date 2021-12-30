#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <vector>
#include <fstream>
#include <iostream>

using namespace std;

struct Node {
    int ID;
    int backoff;
    int R;
    int collisionNum;
};

int N, L, M, R, minR, maxR, T;
vector<int> Rvec;
vector<Node> nodeVec;
int tick=0;
int channlOccupied=0;
int packet_counter;
int utilization_count=0;



int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    // Read from file and initialize variables
    string input_filename = argv[1];
    ifstream input_file;
    input_file.open(input_filename,ios::binary);
    string line;
    if (!input_file) {
        cout << "open file error!" << endl;
    }
    char c;
    input_file >> c >> N >> c >> L >> c >> M >> c ;
    while (input_file.get() != '\n') {
        input_file >> R;
        Rvec.push_back(R);
    }
    input_file >> c >> T;
    minR=Rvec[0];
    maxR = Rvec[M-1];
    packet_counter=L;
    // cout << N << ' ' << L << ' ' << M << ' ' << R << ' ' << T << endl;
    Node node;
    int i;
    for ( i =0; i<N; i++){
        node.ID = i;
        node.collisionNum=0;
        node.R = minR;
        node.backoff=(node.ID)%minR;
        nodeVec.push_back(node);
    }
    int count=0;
    int request_node=0;
    // Start simulation
    while (tick<T){
        // data transfer
        if (channlOccupied==1){
            if (packet_counter<=0){
                packet_counter=L;
                channlOccupied=0;
                
                nodeVec[request_node].collisionNum =0;
                nodeVec[request_node].R = minR;
                nodeVec[request_node].backoff = (nodeVec[request_node].ID+tick)%nodeVec[request_node].R;
            } else {
                cout << "At tick " << tick << ", transfering from node " << request_node <<endl;
                packet_counter-=1;
                utilization_count++;
                tick++;
                continue;
            }
            
        }

        // Channel not occupied
        cout << "Backoffs at tick " << tick << ": ";
        for (i =0;i<N;++i){
                cout << nodeVec[i].backoff << " ";
        }
        cout << endl;
        for (i =0;i<N;++i){
            // count number of 0s
            if (nodeVec[i].backoff<=0){
                count++;
                request_node = i;
            }

        }

        //No collision and some node needs to send
        if (count==1){ 
            channlOccupied=1;
            packet_counter-=1;
            utilization_count++;
            cout << "At tick " << tick << ", transfering from node " << request_node <<endl;
        }

        // Collision occurred
        if (count>1) {
            cout << "At tick " << tick << ", collision ocurred: " << endl;;
            for (i =0;i<N;++i){
                
                // count number of 0s
                if (nodeVec[i].backoff<=0){
                    cout << "Node " << i << ": ";
                    nodeVec[i].collisionNum+=1;
                    if (nodeVec[i].collisionNum<M){
                        nodeVec[i].R=Rvec[nodeVec[i].collisionNum];
                        nodeVec[i].backoff=(nodeVec[i].ID+tick+1)%nodeVec[i].R;
                        
                    } else {
                        nodeVec[i].collisionNum=0;
                        nodeVec[i].R=minR;
                        nodeVec[i].backoff=(nodeVec[i].ID+tick+1)%minR;
                    }
                    cout << "Back off is set to " << nodeVec[i].backoff << endl;
                    
                }

            }

            
            
        }
        if (count==0){
            for (i =0;i<N;++i){
                nodeVec[i].backoff-=1;

            }
        }

        count=0;
        tick++;
    }

    float utilization_rate = float(utilization_count)/float(T);
    
    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fprintf( fpOut, "%.2f" ,utilization_rate );
    fclose(fpOut);
    

    return 0;
}

