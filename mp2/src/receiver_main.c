/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define MAXBUFFLEN 1400 // allow for 2056 bytes
#define FIN_ACK -2
#define END_OF_FILE -2
#define TCP_REQ -1
#define ACK 0
#define MAXFILESIZE 2147483647
#define INFINITLOOPTHRESHOLD 4000
struct sockaddr_in si_me, si_other;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    

    // initialize necessary buffers
    unsigned char packet_buf[MAXBUFFLEN];
    memset(packet_buf, 0, sizeof(packet_buf));

    // buffer for packet sequence
    int header_buf[2];
    memset(header_buf, 0, sizeof(header_buf));


    // buffer for content value
    unsigned char content_buf[MAXBUFFLEN - sizeof(header_buf)];
    memset(content_buf, 0, sizeof(content_buf));

    // initialize necessary variabeles
    struct sockaddr_in tx_addr;
    socklen_t tx_addr_size = sizeof(tx_addr);
    int numbyte_recv;
    int ack_sequence = 0;
    int content_len = 0;

    // open the file for writing
    FILE * fptr = fopen(destinationFile, "wb");

    // array to easily check if a packet has been received or not
    int* packet_received;
    int total_num_packets;
    int num_packets_received = 0;
    int packet_sequence;
    
    

    // Wait for TCP request
    while (1) {
        numbyte_recv = recvfrom(s, packet_buf, sizeof(packet_buf), 0, (struct sockaddr *)&tx_addr, &tx_addr_size);
        if(numbyte_recv < 0){
            diep("error in receiving");
        }
        // else if (numbyte_recv > 0) {
        //     printf("%i of bytes received.\n",numbyte_recv);
        // }
        
        // put header content in header_buf
        memcpy(header_buf, packet_buf, sizeof(header_buf));

        // put data content in content_buf
        memcpy(content_buf, packet_buf + sizeof(header_buf), sizeof(content_buf));

        // case if it is TCP request
        if(header_buf[0] == TCP_REQ){
            
            printf("Received TCP request\n");
            // get total number of packets
            memcpy(&total_num_packets, content_buf, sizeof(int));

            // send ack back
            ack_sequence = -1;
            memcpy(header_buf, &ack_sequence, sizeof(ack_sequence));
            // printf("Sending %i to client\n",header_buf[0]);
            sendto(s, header_buf, sizeof(header_buf), 0, (struct sockaddr *)&tx_addr, tx_addr_size);
            printf("Sending ACK %d to client\n", ack_sequence);
            
            break;
        }   
        if(header_buf[0] == END_OF_FILE){
            
            return;
        }   
    }

    packet_received = (int*) malloc (sizeof(int)*(total_num_packets+1));
    bzero(packet_received,sizeof(int)*(total_num_packets+1));
    ack_sequence = 0;

    // clock_t t;

	/* Now receive data and send acknowledgements */    
    while(num_packets_received < total_num_packets){
        // receive packet first
        numbyte_recv = recvfrom(s, packet_buf, sizeof(packet_buf), 0, (struct sockaddr *)&tx_addr, &tx_addr_size);
        if(numbyte_recv < 0){
            diep("error in receiving");
        }
        // else if (numbyte_recv > 0) {
        //     printf("%i of bytes received.\n",numbyte_recv);
        // }
        
        // t  = clock();
        // put header content in header_buf
        memcpy(header_buf, packet_buf, sizeof(header_buf));

        // put data content in content_buf
        memcpy(content_buf, packet_buf + sizeof(header_buf), sizeof(content_buf));
        printf("Packet %d received\n",header_buf[0]);
        // case if it is TCP request
        if (header_buf[0] == END_OF_FILE){
            printf("FIN received.\n");
            // check if current packets is equal to total number of packets first and end receiving if so
            if(num_packets_received >= total_num_packets){
                printf("All packets received\n");
                free(packet_received);
                break;
            } else {
                printf("At FIN: %d/%d packets received.\n",num_packets_received,total_num_packets);
                memcpy(header_buf, &ack_sequence, sizeof(ack_sequence));
                sendto(s, header_buf, sizeof(header_buf), 0, (struct sockaddr *)&tx_addr, tx_addr_size);
            }

        // case if it is ACK
        } else if (header_buf[0] == ACK) {
            printf("TCP connection established.\n");
            packet_received[0] = 1;
            continue;

        } else { // enter case when it is a packet
            packet_sequence = header_buf[0];
            
            content_len = header_buf[1];
            // printf("Content length = %d\n", content_len);
            if(packet_sequence == ack_sequence + 1){
                // in order packet
                if(packet_received[packet_sequence] == 0){
                    // write to the file
                    fseek(fptr, (packet_sequence-1) * sizeof(content_buf), SEEK_SET);
                    fwrite(content_buf, sizeof(char), content_len, fptr);
                    packet_received[packet_sequence] = 1;
                    num_packets_received += 1;
                    // printf("num_packets_received: %d\n", num_packets_received);
                    // printf("Ack sequence before increasing: %d\n",ack_sequence);
                }

                // find next ack to send
                int i;
                for (i = 1;packet_received[i]!=0;i++){
                    ack_sequence = i;
                }

                // while(packet_received[ack_sequence + 1] != 0){
                //     ack_sequence++;
                // }
                // printf("Ack sequence after increasing: %d\n",ack_sequence);
                

            } else if(header_buf[0] > ack_sequence + 1){
                // out of order packet
                if(packet_received[packet_sequence] == 0){
                    // write to the file
                    fseek(fptr, (packet_sequence-1)* sizeof(content_buf), SEEK_SET);
                    fwrite(content_buf, sizeof(char), content_len, fptr);
                    packet_received[packet_sequence] = 1;
                    num_packets_received += 1;
                    // printf("num_packets_received: %d\n", num_packets_received);
                }
            }
            memcpy(header_buf, &ack_sequence, sizeof(ack_sequence));
            // printf("Sending ACK %d to client after %d ms elapsed\n", ack_sequence,(int)difftime(clock(),t));
            sendto(s, header_buf, sizeof(header_buf), 0, (struct sockaddr *)&tx_addr, tx_addr_size);
            // printf("\n");
        }

    }

    // send FIN_ACK
    ack_sequence = FIN_ACK;
    memcpy(header_buf, &ack_sequence, sizeof(ack_sequence));
    sendto(s, header_buf, sizeof(header_buf), 0, (struct sockaddr *)&tx_addr, tx_addr_size);

    // loop incase FIN_ACK gets lost
    time_t t0 = clock();
    while(1){
        printf("Closing Connection.\n");
        numbyte_recv = recvfrom(s, packet_buf, sizeof(packet_buf), 0, (struct sockaddr *)&tx_addr, &tx_addr_size);
        if(numbyte_recv < 0){
            diep("error in receiving");
        } 
        
        // put header content in header_buf
        memcpy(header_buf, packet_buf, sizeof(header_buf));

        // put data content in content_buf
        memcpy(content_buf, packet_buf + sizeof(header_buf), sizeof(content_buf));

        if(header_buf[0] == ACK){
            break;
        } else if(header_buf[0] == END_OF_FILE){
            ack_sequence = FIN_ACK;
            memcpy(header_buf, &ack_sequence, sizeof(ack_sequence));
            sendto(s, header_buf, sizeof(header_buf), 0, (struct sockaddr *)&tx_addr, tx_addr_size);
            break;
        }
        if (difftime(clock(),t0)>INFINITLOOPTHRESHOLD) {
            break;
        }    
    }


    fclose(fptr);
    close(s);
	printf("%s received.\n", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}