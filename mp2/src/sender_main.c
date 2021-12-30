/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#define MAXPACKSIZE 1400
#define HEADERSIZE 8
#define SS 0
#define FR 1
#define CA 2
#define DEFAULTTIMEOUT 500
#define DEFAULTTIMEOUTSTD 30
#define MAXFILESIZE 2147483647
#define RTTLENGTH 20
#define INFINITLOOPTHRESHOLD 4000

struct sockaddr_in si_other;
int s;
unsigned int slen;
void diep(char *s) {
    perror(s);
    exit(1);
}

int parse_ACK (const char* buffer) {
    if (buffer==NULL) return -3;
    int ACK_num;
    memcpy(&ACK_num,buffer, 4);
    return ACK_num; // -3 indicates error 
}

int min(unsigned long long int a,unsigned long long int b){
    if (a>b) return b;
    return a;
}

int min_float(float a,float b){
    if (a>b) return b;
    return a;
}

float increment(float CW, float SST, int current_ACK, int new_ACK, int state){
    if (CW<1) CW=1;
    if (SST<1) SST=1;
    float increment;
    float k=2;
    if (state==0){ // slow start
        increment = k*(float) (new_ACK-current_ACK);
    }
    else if (state==2) { // congestion avoidance
        // increment = k*(new_ACK-current_ACK)/ (float)((int)CW) ;
        increment = (float) (new_ACK-current_ACK);
    }
    else {
        increment = 1;
    }
    // printf("Increment = %f.\n",increment);
    return increment;
}

double increment_timer(int current_ACK,int last_ACK,double* timer_array, int num_packets){
    if (timer_array==NULL) return 0;
    if (current_ACK>=num_packets) return 0;
    double increment=0;
    int i;
    for (i=last_ACK+1; i<current_ACK; i++){
        increment  += timer_array[i];
    }
    return increment;
    
}

/* Send packets based on CW
 *  packet_array: array of packets starting from packet 1; Each packet contains 4 bytes of header, 1395 bytes of data, and 1 byte of NULL
 *  num_packets:  total number of packets in the packet array
 *  last_sent: pointer to the integer that keeps track of the last packet # that has been sent
 *  time_array: array that keeps track of last sending times of each packet; Use time_t current_time = clock() as the current clock; 
 *                      Update this array each time you send packets
*/
void send_packets(char* packet_array,int CW_base,float CW,int num_packets, int* last_sent, time_t* time_array){
    int i;
    if (CW<1) CW = 1;
    if(*last_sent < CW_base + (int)CW){
        for(i = *last_sent + 1; i < CW_base + (int)CW ; i++){
            if(i > num_packets){
                break;
            }
            //printf("Sending packet %d to receiver.\n",i);
            sendto(s, packet_array + (i-1) * (MAXPACKSIZE), MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
            time_array[i-1] = clock();
            // if (i==1){
            //     timer_array[i-1]=0;
            // } else if (i>1) {
            //     timer_array[i-1]=(double)difftime(clock(),time_array[i-2]);
            // }

            printf("Packet %d is sent at time %d\n",i,(int)time_array[i-1]);
            *last_sent = i;
        }
    }
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    char buffer[MAXPACKSIZE];
    int seq_num;
    
    int recvLen;
    time_t t0;
    time_t t_start;
    time_t current_time;
    double timeout = DEFAULTTIMEOUT;

    t_start = clock();

    if (bytesToTransfer>(unsigned long long)MAXFILESIZE){
        if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            diep("socket");
        memset((char *) &si_other, 0, sizeof (si_other));
        si_other.sin_family = AF_INET;
        si_other.sin_port = htons(hostUDPport);
        if (inet_aton(hostname, &si_other.sin_addr) == 0) {
            fprintf(stderr, "inet_aton() failed\n");
            exit(1);
        }


        seq_num = -2; // ACK
        memset(buffer, 0, sizeof(buffer));
        memcpy (buffer, &seq_num, 4 );
        t0 = clock();
        sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
        while (1){
            // Wait for ACK -2 
            recvLen = recvfrom(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, &slen);
            current_time = clock();
            if (recvLen>0){
                buffer[recvLen] = 0;
                if (parse_ACK(buffer)==seq_num){
                    printf("Received ACK-TCP close from server.\n");
                    break;
                }
            
            }
            // printf("Time passed: %f ms\n", difftime(current_time,t0));
            if (difftime(current_time,t0)>timeout) { // timeout
                printf("TCP close timeout.\n");
                memset(buffer, 0, sizeof(buffer));
                memcpy (buffer, &seq_num, 4 );
                sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other)); 
                t0 = clock();
            }
            
        }
        sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
        close(s);
    }


	/* Determine how many bytes to transfer */
    unsigned long long lSize; // File size in bytes
    char * file_buffer;
    unsigned long long file_size; // File size in bytes
    // File copy reference: http://www.cplusplus.com/reference/cstdio/fread/
    // obtain file size:
    fseek (fp , 0 , SEEK_END);
    lSize = ftell (fp);
    rewind (fp);
    // allocate memory to contain the whole file:
    file_buffer = (char*) malloc (sizeof(char)*lSize);
    if (file_buffer == NULL) {fputs ("Memory error",stderr); exit (2);}
    // copy the file into the buffer:
    file_size = fread (file_buffer,1,lSize,fp);
    if (file_size != lSize) {fputs ("Reading error",stderr); exit (3);}
    fclose (fp);
    printf("File read from directory\n");

    /* make socket */
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");
    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

	/* Send data and receive acknowledgements on s*/

    
    
    // struct sockaddr_in fromAddr;

    // set non blocking socket
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 1;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);


    /* Prepare packets */
    printf("Preparing packets..\n");
    char* packet_array;
    int packet_data_size = MAXPACKSIZE-HEADERSIZE;// transfer 1392 bytes in each packet
    bytesToTransfer = min(bytesToTransfer,file_size);
    int num_packets = (bytesToTransfer+packet_data_size-1)/(packet_data_size); // ceiling division
    int packet_seq;
    int remaining_bytes = bytesToTransfer;
    packet_array = (char*) malloc (sizeof(char)*(num_packets*MAXPACKSIZE));
    memset(packet_array,0,sizeof(char)*(num_packets*MAXPACKSIZE));
    int i;
    for ( i = 0; i<num_packets; i++){
        packet_seq = i+1;
        memcpy(packet_array+i*MAXPACKSIZE,&packet_seq,HEADERSIZE);
        if (i*packet_data_size<file_size){
            if (remaining_bytes<packet_data_size) {
                memcpy(packet_array+i*MAXPACKSIZE+HEADERSIZE,file_buffer+i*packet_data_size,remaining_bytes);
                memcpy(packet_array+i*MAXPACKSIZE+4,&remaining_bytes,sizeof(remaining_bytes));
                // printf("Size of packet %i: %i\n",packet_seq,remaining_bytes);
                remaining_bytes -=remaining_bytes;
            }
            else {
                memcpy(packet_array+i*MAXPACKSIZE+HEADERSIZE,file_buffer+i*packet_data_size,packet_data_size);
                memcpy(packet_array+i*MAXPACKSIZE+4,&packet_data_size,sizeof(packet_data_size));
                remaining_bytes -=packet_data_size;
                // printf("Size of packet %i: %i\n",packet_seq,packet_data_size);
            }
        } else{
            if (remaining_bytes<packet_data_size) {
                memcpy(packet_array+i*MAXPACKSIZE+4,&remaining_bytes,sizeof(remaining_bytes));
                // printf("Size of packet %i: %i\n",packet_seq,remaining_bytes);
                remaining_bytes -=remaining_bytes;
            } else{
                memcpy(packet_array+i*MAXPACKSIZE+4,&packet_data_size,sizeof(packet_data_size));
                // printf("Size of packet %i: %i\n",packet_seq,packet_data_size);
                remaining_bytes -=packet_data_size;
            } 
        }
    }
    free (file_buffer);

    // Set up connection
    seq_num = -1; // SYN
    memset(buffer, 0, sizeof(buffer));
    memcpy (buffer, &seq_num, 4 );
    memcpy (buffer+HEADERSIZE, &num_packets, 4 );
    
    sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other)); 
    printf("Sent TCP request to receiver\n");
    t0 = clock();
    timeout = DEFAULTTIMEOUT;
    while (1){
        // Wait for ACK
        recvLen = recvfrom(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, &slen);
        current_time = clock();
        if (recvLen>0){
            buffer[recvLen] = 0;
            if (parse_ACK(buffer)==seq_num){
                printf("Received SYN-ACK from server.\n");
                break;
            }
           
        }
        // printf("Time passed: %f ms\n", difftime(current_time,t0));
        if (difftime(current_time,t0)>timeout) { // timeout
            sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other)); 
        }
        
    }

    // Send ACK
    seq_num = 0; // ACK
    memset(buffer, 0, sizeof(buffer));
    memcpy (buffer, &seq_num, 4 );
    sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));

    /* Start TCP state machine */
    float CW =1;
    float SST = 64;
    int DupACK = 0;
    int last_ACK = 0;
    int current_ACK = 0;
    int CW_base = 1;
    int current_state = SS;
    int last_sent = 0;
    // double timer=timeout;
    // double time_passed =0;
    
    double rtt = 0;
    clock_t* time_array;
    // double* timer_array;
    time_array=(time_t*) malloc (sizeof(time_t)*(num_packets));
    // timer_array=(double*) malloc (sizeof(double)*(num_packets));
    if (time_array == NULL) {fputs ("Memory error",stderr); exit (2);}
    // if (timer_array == NULL) {fputs ("Memory error",stderr); exit (2);}
    memset(time_array, 0 , (sizeof(time_t)*(num_packets)));
    // memset(timer_array, 0 , (sizeof(double)*(num_packets)));

    double rtt_table[RTTLENGTH];
    int j ;
    for (j=0 ; j<RTTLENGTH;j++){
        rtt_table[j] = DEFAULTTIMEOUT;
    }


    // time_t last_time = clock();
    while (current_ACK<num_packets) {
        send_packets(packet_array,CW_base,CW,num_packets,&last_sent, time_array);
        bzero(buffer, MAXPACKSIZE);
        // time_passed = (double) difftime(clock(),last_time);
        // timer = timer-time_passed;
        // last_time = clock();
	    // printf("Current CW: %f; Current state: %d; Current SST:%f\n",CW,current_state,SST);
        switch (current_state)
        {
            case SS:
                
                recvLen = recvfrom(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, &slen);
                if (recvLen>0){
                    current_ACK =parse_ACK(buffer);
                    printf("Current ACK %d; Current CW: %f; Current state: %d; Current SST:%f\n",current_ACK,CW,current_state,SST);
                    // printf("Received ACK %d at state %d at %d when waiting for %d\n", current_ACK, current_state, (int)clock(), CW_base);
                    if (current_ACK>=CW_base-1){ // ACKs of interest

                        if (current_ACK==last_ACK) { // DupACK
                            DupACK++;
                            if (DupACK==3){
                                SST = CW /2;if (SST<1) SST=1;
                                CW = SST+3;
                                memcpy(&j, packet_array + current_ACK * (MAXPACKSIZE),sizeof(int));
                                printf("Sending dupack packet %d to receiver.\n",j);
                                
                                sendto(s, packet_array + current_ACK * (MAXPACKSIZE), MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
                                // timer = timeout;
                                time_array[current_ACK] = clock();
                                send_packets(packet_array,CW_base,CW,num_packets,&last_sent, time_array);
                                current_state = FR;
                            }
                            continue;
                        } else { // New ACK
                            
                            printf("RTT for packet %i: %f ms\n",current_ACK,rtt);
                            rtt = (double) difftime(clock(),time_array[current_ACK-1]);
                            timeout  = 0;
                            rtt_table[(current_ACK-1)%10]=rtt;
                            for (j = 0; j <RTTLENGTH; j++) {
                                timeout+= rtt_table[j];
                            }
                            timeout = min_float(timeout/(float)RTTLENGTH+DEFAULTTIMEOUTSTD,DEFAULTTIMEOUT);
                            printf("Current timeout: %f\n",timeout);
                            CW = CW+increment(CW,  SST,  last_ACK,  current_ACK,  current_state);
                            CW_base = current_ACK+1;

                            send_packets(packet_array,CW_base,CW,num_packets,&last_sent, time_array);
                            // timer = timer+increment_timer(current_ACK,last_ACK,timer_array,num_packets);
                            DupACK =0;
                            last_ACK = current_ACK;
                            if (CW>=SST) current_state = CA;
                            continue;
                        }
                        
                    }
                    else { 
                        continue;
                    }
                    
                }
                else { // detect timeout
                    current_time = clock();
                    if (current_time -time_array[CW_base-1]>=timeout){
                        printf("Timeout for packet %d detected. \n",CW_base);
                        printf("Current CW: %f; Current state: %d; Current SST:%f\n",CW,current_state,SST);
                        printf("Sent time for packet %d: %d; Current time: %d\n\n",CW_base,(int)time_array[CW_base-1],(int)current_time);
                        // timer = timeout;
                        SST=CW/2;if (SST<1) SST=1;
                        CW =1;
                        DupACK = 0;
                        memcpy(&j, packet_array + (CW_base-1) * (MAXPACKSIZE),sizeof(int));
                        printf("Sending timeout packet %d to receiver.\n",j);
                        sendto(s, packet_array + (CW_base-1) * (MAXPACKSIZE), MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
                        // timer = timeout;
                        time_array[CW_base-1] = clock();
                    }
                }
            case FR:
                
                recvLen = recvfrom(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, &slen);

                if(recvLen > 0 ){

                    current_ACK = parse_ACK(buffer);
                    
                    printf("Current ACK %d; Current CW: %f; Current state: %d; Current SST:%f\n",current_ACK,CW,current_state,SST);
                    // printf("Received ACK %d at state %d at %d when waiting for %d\n", current_ACK, current_state, (int)clock(), CW_base);
                    if (current_ACK>=CW_base-1){
                        if(current_ACK == last_ACK){
                            // dupAck state again
                            DupACK++;
                            CW++;
                            send_packets(packet_array, CW_base, CW, num_packets, &last_sent, time_array);

                        } else {
                            // newAck state event, move to congest avoidance
                            // timer = timer+increment_timer(current_ACK,last_ACK,timer_array,num_packets);
                            CW = SST;
                            DupACK = 0;
                            last_ACK = current_ACK;
                            CW_base = current_ACK+1;
                            printf("RTT for packet %i: %f ms\n",current_ACK,rtt);
                            rtt = (double) difftime(clock(),time_array[current_ACK-1]);
                            timeout  = 0;
                            rtt_table[(current_ACK-1)%10]=rtt;
                            for (j = 0; j <RTTLENGTH; j++) {
                                timeout+= rtt_table[j];
                            }
                            timeout = min_float(timeout/(float)RTTLENGTH+DEFAULTTIMEOUTSTD,DEFAULTTIMEOUT);
                            printf("Current timeout: %f\n",timeout);
                            send_packets(packet_array, CW_base, CW, num_packets, &last_sent, time_array);
                            current_state = CA;
                        }
                    }
                    
                }
                else{
                // check for timeout if no packets arriving
                    current_time = clock();
                    if (current_time -time_array[CW_base-1]>=timeout){
                        printf("Timeout for packet %d detected. \n",CW_base);
                        printf("Current CW: %f; Current state: %d; Current SST:%f\n",CW,current_state,SST);
                        printf("Sent time for packet %d: %d; ACK time: %d\n\n",CW_base,(int)time_array[CW_base-1],(int)current_time);
                        SST=CW/2;if (SST<1) SST=1;
                        CW =1;
                        DupACK = 0;
                        memcpy(&j, packet_array + (CW_base-1) * (MAXPACKSIZE),sizeof(int));
                        printf("Sending timeout packet %d to receiver.\n",j);
                        sendto(s, packet_array + (CW_base-1) * (MAXPACKSIZE), MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
                        // timer = timeout;
                        time_array[CW_base-1] = clock();
                        current_state = SS;
                    }
                }
                continue;
            case CA:
               
                recvLen = recvfrom(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, &slen);
                if (recvLen>0){
                    current_ACK =parse_ACK(buffer);
                    printf("Current ACK %d; Current CW: %f; Current state: %d; Current SST:%f\n",current_ACK,CW,current_state,SST);
                    // printf("Received ACK %d at state %d at %d when waiting for %d\n", current_ACK, current_state, (int)clock(), CW_base);
                    
                    if (current_ACK>=CW_base-1){ 
                        if (current_ACK==last_ACK) {
                            DupACK++;
                            if (DupACK==3){
                                SST = CW /2;if (SST<1) SST=1;
                                CW = SST+3;
                                memcpy(&j, packet_array + current_ACK * (MAXPACKSIZE),sizeof(int));
                                printf("Sending dupack packet %d to receiver.\n",j);
                                sendto(s, packet_array + current_ACK * (MAXPACKSIZE), MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
                                time_array[current_ACK] = clock();
                                // timer = timeout;
                                send_packets(packet_array,CW_base,CW,num_packets,&last_sent, time_array);
                                current_state = FR;
                            }
                            continue;
                        } else {// new ACK
                            if (CW>=1) {
                                CW = CW+increment(CW,  SST,  last_ACK,  current_ACK,  current_state);
                            }
                            else CW = 1;
                            CW_base = current_ACK+1;
                            rtt = (double) difftime(clock(),time_array[current_ACK-1]);
                            timeout  = 0;
                            rtt_table[(current_ACK-1)%10]=rtt;
                            for (j = 0; j <RTTLENGTH; j++) {
                                timeout+= rtt_table[j];
                            }
                            timeout = min_float(timeout/(float)RTTLENGTH+DEFAULTTIMEOUTSTD,DEFAULTTIMEOUT);
                            printf("Current timeout: %f\n",timeout);
                            // timer = timer+increment_timer(current_ACK,last_ACK,timer_array,num_packets);
                            send_packets(packet_array,CW_base,CW,num_packets,&last_sent, time_array);
                            DupACK =0;
                            printf("RTT for packet %i: %f ms\n",current_ACK,difftime(clock(),time_array[current_ACK-1]));
                            last_ACK = current_ACK;
                            continue;
                        }
                        
                    }
                    
                    
                }
                else { // detect timeout
                    current_time = clock();
                    if (current_time -time_array[CW_base-1]>=timeout){
                        printf("Timeout for packet %d detected.\n",CW_base);
                        printf("Current CW: %f; Current state: %d; Current SST:%f\n",CW,current_state,SST);
                        printf("Sent time for packet %d: %d; ACK time: %d\n\n",CW_base,(int)time_array[CW_base-1],(int)current_time);
                        SST=CW/2; if (SST<1) SST=1;
                        CW =1;
                        DupACK = 0;
                        memcpy(&j, packet_array + (CW_base-1) * (MAXPACKSIZE),sizeof(int));
                        printf("Sending timeout packet %d to receiver.\n",j);
                        sendto(s, packet_array + (CW_base-1) * (MAXPACKSIZE), MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
                        // timer = timeout;
                        time_array[CW_base-1] = clock();
                        current_state = SS;
                    }
                }
                continue;
            default:
                continue;
        }
    }
    
    /* Send TCP Close */
    printf("All packets transfered. Prepare to close TCP.\n");
    seq_num = -2; // ACK
    memset(buffer, 0, sizeof(buffer));
    memcpy (buffer, &seq_num, 4 );
    t0 = clock();
    time_t t1 = clock();
    sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));
    while (1){
        // Wait for ACK -2 
        recvLen = recvfrom(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, &slen);
        current_time = clock();
        if (recvLen>0){
            buffer[recvLen] = 0;
            if (parse_ACK(buffer)==seq_num){
                printf("Received ACK-TCP close from server.\n");
                break;
            }
           
        }
        // printf("Time passed: %f ms\n", difftime(current_time,t0));
        if (difftime(current_time,t0)>timeout) { // timeout
            printf("TCP close timeout.\n");
            memset(buffer, 0, sizeof(buffer));
            memcpy (buffer, &seq_num, 4 );
            sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other)); 
            t0 = clock();
        }
        if (difftime(current_time,t1)>INFINITLOOPTHRESHOLD) {
            break;
        }    
        
    }
    sendto(s, buffer, MAXPACKSIZE, 0, (struct sockaddr*)&si_other, sizeof(si_other));

    printf("Closing the socket\n");
    close(s);
    free(packet_array);
    // free(timer_array);
    // if (time_array){
    //     free(time_array);
    // }
    printf("Transfer of %d bytes takes %f seconds. Transmission speed:%f Mbps.\n",(int)bytesToTransfer,(float)difftime(clock(),t_start)/(float)1000,(float)bytesToTransfer/1000000/(float)difftime(clock(),t_start));
    return;

}

/*
 * 
 */

// argv[0] = sender.obj;
// argv[1] = receiver_hostname
// argv[2] = receiver_port
// argv[3] = filename_to_xfer
// argv[4] = bytes_to_xfer

int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    printf("Entering reliable transfer\n");

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}