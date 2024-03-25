#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 8080

/*global data relative to this node*/

int id;
int sequence = 0;

/*---------------------------------*/

typedef int payload_t;

typedef struct Packet {
    int source;
    int sequence;
    payload_t payload;
} Packet;

/*2sec window must keep about 200 packets*/
Packet* packet_window;/*points to the first packet of the list of received packets*/
int pw_len = 0;/*packet_window current length */

Packet prepare_packet();

int generate_traffic(int ms, int time, int sock, struct sockaddr_in* addr, socklen_t* addrlen);/*generate traffic every 'ms' milliseconds*/
int broadcaster(int sock, struct sockaddr_in* addr, socklen_t* addrlen);/*send packets to all the nodes*/
int traffic_analyzer(Packet* buffer, int len);/*prints a traffic report*/


/**
 * Sleep a given amount of milliseconds
 */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}


int main(int argc, char* argv[]) {

    // Autoflush stdout for docker
    setvbuf(stdout, NULL, _IONBF, 0);/*perform a write on stdout immediately after an output operation is performed. Useful in case of log operations*/

    if(argc != 2){

        puts("Not enough parameter");
        exit(EXIT_FAILURE);
    }

    id = atoi(argv[1]);

    srand(time(NULL)); /*rand seed (to call one time only)*/

    const int optval = 1;/*SO_BROADCAST argument*/
    struct timeval timeo; /*SO_RCVTIMEO argument*/
    timeo.tv_sec = 0;
    timeo.tv_usec = 12000; /*wait 12ms*/

    int sock;
    
    struct sockaddr_in addr;
    socklen_t addrlen = (socklen_t) sizeof(addr);

    /*NODE SETUP*/

    /*socket setup*/
    sock = socket(AF_INET, SOCK_DGRAM, 0);/*udp socket*/

    if(sock == -1){/*error check*/

        perror("Unable to create the socket ");
        exit(EXIT_FAILURE);
    }

    /*socket options + error check*/
    if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1){/*option managed at socket level (SOL_SOCKET). SO_BROADCAST option is set to true (optval, see man(7) socket)*/

        perror("Setsockopt error ");
        exit(EXIT_FAILURE);
    }

    /*timeout afetr .. sec. This way, recvfrom will not block the execution*/
    if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo)) == -1){/*option managed at socket level (SOL_SOCKET). SO_BROADCAST option is set to true (optval, see man(7) socket)*/

        perror("Setsockopt error ");
        exit(EXIT_FAILURE);
    }

    /*set the address (addr)*/
    memset(&addr, 0, sizeof(addr));/*fills addr with 0*/
    addr.sin_family = AF_INET;/*allows communications over the network*/
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);/*uses broadcast address, converted from host byte order to network byte order*/
    addr.sin_port = htons(PORT);/*set 8080 port as default*/

    /*bind socket to an ip address (INADDR_BROADCAST)*/
    if(bind(sock, (const struct sockaddr*) &addr, (socklen_t) sizeof(addr)) == -1){

        perror("Unable to bind ");
        exit(EXIT_FAILURE);
    }
    

    // Traffic generator
    /*send a packet every 10ms, for 20 seconds*/
    generate_traffic(10, 20, sock, &addr, &addrlen);

    // Broadcaster

    // Traffic analyzer
    /*
    print every sec report of received pkts as follow "1: 10, 2: 008, 3: 015, 4: 085, ..., 10: 100"
    */

    if(packet_window){free(packet_window);}

    return 0;
}

Packet prepare_packet(){/*generate a message. I keep track of the messages for the sliding window*/

    Packet p;

    p.source = id;
    p.sequence = sequence++;

    /*packet content*/
    p.payload = (payload_t) rand();/*generates a pseudo-random number between 0 and RAND_MAX*/

    return p;
}

int generate_traffic(int ms, int time, int sock, struct sockaddr_in* addr, socklen_t* addrlen){/*generate traffic every 'ms' milliseconds for 'time' sec*/

    int ret;
    for(int i = 0; i < time; i++){

        Packet total_traffic[100];

        for(int j = 0; j < 100; j++){

            total_traffic[j] = prepare_packet();
        }

        for(int j = 0; j < 100; j++){

            ret = sendto(sock, &total_traffic[j], sizeof(Packet), 0, (const struct sockaddr*) addr, (socklen_t) sizeof(*addr));
            if(ret == -1){/*no need of flags, hence the 0 in the arguments*/
    
                perror("sendto error ");
                return -1;
            }

            msleep(ms);
        }

        /*receive packet (read the buffer) from broadcaster and send to traffic analyzer*/
        broadcaster(sock, addr, addrlen);
    }
}

int broadcaster(int sock, struct sockaddr_in* addr, socklen_t* addrlen){

    int buflen = 100;
    Packet buffer[buflen]; /*i expect to read about 100 packets*/

    int ret, actual_buflen = buflen;/*actual_buflen is the amount of packet actually read*/
    for(int i = 0; i < buflen; i++){

        ret = recvfrom(sock, &buffer[i], sizeof(Packet), 0 , (struct sockaddr*) addr, addrlen);

        if(ret == -1){

            if(errno == EAGAIN){/*there is no data to read in the buffer*/

                break;
            }

            perror("recvfrom error ");
            return -1;
        }

        if(ret == 0){

            break;
        }

        actual_buflen = i;
    }

    traffic_analyzer(buffer, actual_buflen);
}

/*len is the length of the buffer*/
int traffic_analyzer(Packet* buffer, int len){

    int total_recv[10];
    for(int i = 0; i < 10; i++){total_recv[i] = 0;}

    /*load values into total_recv*/
    for(int i = 0; i < len; i++){

        if(buffer[i].source > -1){

            total_recv[buffer[i].source]++;
        }
    }

    /*print the report*/
    for(int i = 0; i < 10; i++){

        printf("%02d: %03d, ", i + 1, total_recv[i]);
    }
    putchar('\n');

    return 0;
}
