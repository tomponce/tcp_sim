#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER "129.120.151.96" // cse03
#define CLIENT "129.120.151.97" // cse04
#define BUFLEN 512  //Max length of buffer

void die(char *s)
{
    perror(s);
    exit(1);
}

struct tcp_segment
{
    unsigned short int src_port;
    unsigned short int dest_port;
    unsigned int seq;
    unsigned int ack;
    unsigned short int hdr_flags;
    unsigned short int rec;
    unsigned short int checksum;
    unsigned short int ptr;
    char payload [BUFLEN];
};

unsigned short checksum(struct tcp_segment *segment);

void segprint(struct tcp_segment *mysegment, FILE *fd);

int main()
{
    struct sockaddr_in si_server, si_client;
    struct tcp_segment srv_segment, cli_segment;
    int listen_fd, conn_fd, seglen = sizeof(srv_segment), slen = sizeof(si_server) , recv_len, n;
    unsigned short portno;
    char recvline[BUFLEN];
    FILE *f_write, *f_log;
    srand(time(0)); //for random seq number

    //get destination port number
    printf("Port: ");
    scanf("%hu", &portno);

    //create a TCP socket descriptor
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        die("socket");
    }
    // zero out the structure
    memset((char *) &si_server, 0, slen);
    // fill in sock struct segments
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(portno);
    //si_server.sin_addr.s_addr = htons(INADDR_ANY);
    if (inet_aton(SERVER , &si_server.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    //bind socket to port
    if(bind(listen_fd, (struct sockaddr*)&si_server, slen ) == -1)
    {
        die("bind");
    }

    f_log = fopen("server.out", "a");
    if (f_log == NULL)
    {
        die("file");
    }
    
    memset((char *) &srv_segment, 0, seglen);  

    //listen for incoming connection
    printf("Listening...\n");
    if(listen(listen_fd, 10) < 0)
    {
        die("listen");
    }
    if((conn_fd = accept(listen_fd, (struct sockaddr*)NULL, NULL)) < 0)
    {
        die("accept");
    }
    printf("Connected\n");
    bzero(&cli_segment, seglen);
    if(read(conn_fd, &cli_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&cli_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    
    if((cli_segment.hdr_flags & 0x003F) != 0x0002) //check flags (last 6 bits);
    {
        printf("Connection not established!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }

    //print segments
    printf("Received SYN!\n");
    fprintf(f_log, "Received SYN!\n");   //write to log
    segprint(&cli_segment, f_log);

    //prepare to send ack
    memset((char *) &srv_segment, 0, seglen);
    srv_segment.src_port = portno;
    srv_segment.dest_port = cli_segment.src_port;
    srv_segment.seq = rand();
    srv_segment.ack = cli_segment.seq + 1;
    srv_segment.hdr_flags = 0x5012; //header length = 5, set SYN and ACK flag to 1; 0x5012 = 0101 0000 0001 0010
    srv_segment.checksum = checksum(&srv_segment);
    
    sleep(1);

    //print segments
    printf("\nSending SYNACK...\n");
    fprintf(f_log, "\nSending SYNACK...\n");//write to log
    segprint(&srv_segment, f_log);

    //send
    if(write(conn_fd, &srv_segment, seglen) == -1)
    {
        die("write()");
    }

    //await ACK for SYNACK
    if(read(conn_fd, &cli_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&cli_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    
    if((cli_segment.hdr_flags & 0x003F) != 0x0002) //check flags (last 6 bits);
    {
        printf("Connection not established!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }

    //print segments
    printf("Connection Established!\n");
    fprintf(f_log, "Connection Established!\n");//write to log
    segprint(&cli_segment, f_log);
    
    //close connection
    if(read(conn_fd, &cli_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&cli_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    
    if((cli_segment.hdr_flags & 0x003F) != 0x0001) //check flags (last 6 bits);
    {
        printf("Expected conection close\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }

    printf("Connection close request received!\n");
    fprintf(f_log, "Connection close request received!\n");
    segprint(&cli_segment, f_log);

    //send close ack
    //prepare to send ack
    memset((char *) &srv_segment, 0, seglen);
    srv_segment.src_port = portno;
    srv_segment.dest_port = cli_segment.src_port;
    srv_segment.seq = 1024;
    srv_segment.ack = cli_segment.seq + 1;
    srv_segment.hdr_flags = 0x5002; //header length = 5, set SYN and ACK flag to 1; 0x5012 = 0101 0000 0001 0010
    srv_segment.checksum = checksum(&srv_segment);

    sleep(1);

    printf("Sending ACK for close...\n");
    fprintf(f_log, "Sending ACK for close...\n");
    segprint(&srv_segment, f_log);

    //send
    if(write(conn_fd, &srv_segment, seglen) == -1)
    {
        die("write()");
    }

    //close
    memset((char *) &srv_segment, 0, seglen);
    srv_segment.src_port = portno;
    srv_segment.dest_port = cli_segment.src_port;
    srv_segment.seq = 1024;
    srv_segment.ack = cli_segment.seq + 1;
    srv_segment.hdr_flags = 0x5001; //header length = 5, set SYN and ACK flag to 1; 0x5012 = 0101 0000 0001 0010
    srv_segment.checksum = checksum(&srv_segment);

    sleep(1);

    printf("Closing connection...\n");
    fprintf(f_log, "Closing connection...\n");
    segprint(&srv_segment, f_log);

    //send
    if(write(conn_fd, &srv_segment, seglen) == -1)
    {
        die("write()");
    }

    //receive closure ACK
    if(read(conn_fd, &cli_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&cli_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    
    if((cli_segment.hdr_flags & 0x003F) != 0x0002) //check flags (last 6 bits);
    {
        printf("Expected connection close\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }

    printf("Connection closure acknowledged!\n");
    fprintf(f_log, "Connection closure acknowledged!\n");
    segprint(&cli_segment, f_log);

    close(conn_fd);
    close(listen_fd);
    fclose(f_log);
    
}

unsigned short checksum(struct tcp_segment *tcp_seg)
{
    unsigned short int cksum_arr[266];
    unsigned int i,sum=0, cksum, wrap;
    memcpy(cksum_arr, tcp_seg, 532); // Copying 532 bytes

    for (i=0;i<266;i++)            // Compute sum
    {
        sum = sum + cksum_arr[i];
    }

    wrap = sum >> 16;             // Get the carry
    sum = sum & 0x0000FFFF;       // Clear the carry bits
    sum = wrap + sum;             // Add carry to the sum

    wrap = sum >> 16;             // Wrap around once more as the previous sum could have generated a carry
    sum = sum & 0x0000FFFF;
    cksum = wrap + sum;

    /* XOR the sum for checksum */
    cksum = 0xFFFF^cksum;
    return cksum;
}

void segprint(struct tcp_segment *mysegment, FILE *fd)
{
    printf("Source Port: %d; Destination Port: %d\n", mysegment->src_port, mysegment->dest_port);
    printf("Sequence number: %d; Acknowledgement number: %d\n", mysegment->seq, mysegment->ack);
    printf("Header flags: %d (0x%04X)\n", mysegment->hdr_flags, mysegment->hdr_flags);
    printf("Checksum: %d (0x%04X)\n\n", mysegment->checksum, mysegment->checksum);

    //write to log file
    fprintf(fd, "Source Port: %d; Destination Port: %d\n", mysegment->src_port, mysegment->dest_port);
    fprintf(fd, "Sequence number: %d; Acknowledgement number: %d\n", mysegment->seq, mysegment->ack);
    fprintf(fd, "Header flags: %d (0x%04X)\n\n", mysegment->hdr_flags, mysegment->hdr_flags);
    fprintf(fd, "Checksum: %d (0x%04X)\n\n", mysegment->checksum, mysegment->checksum);
}