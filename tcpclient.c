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

void segprint(struct tcp_segment *mysegment, FILE *file);

int main()
{
    struct sockaddr_in si_server, si_client;
    struct tcp_segment srv_segment, cli_segment;
    int conn_fd, cli_fd, seglen = sizeof(srv_segment), slen = sizeof(si_server) , recv_len, n;
    unsigned short portno;
    char recvline[BUFLEN];
    FILE *f_write, *f_log;
    
    f_log = fopen("client.out", "a");
    if (f_log == NULL)
    {
        die("file");
    }

    //get destination port number
    printf("Port: ");
    scanf("%hu", &portno);

    //create a TCP socket descriptor
    if ((conn_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        die("socket");
    }
    // zero out the structure
    memset((char *) &si_server, 0, slen);
    // fill in sock struct segments
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(portno);
    if (inet_aton(SERVER, &si_server.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    //connect to server
    if(connect(conn_fd, (struct sockaddr *)&si_server,sizeof(si_server)) < 0)
    {
        die("accept");
    }

    //get client port number and IP address
    int clen = sizeof(si_client);
    getsockname(cli_fd, (struct sockaddr *) &si_client, &clen);
    //populate segment
    memset((char *) &cli_segment, 0, seglen);
    cli_segment.src_port = ntohs(si_client.sin_port);
    cli_segment.dest_port = portno;
    srand(time(0));
    cli_segment.seq = rand();
    cli_segment.hdr_flags = 0x5002; //header length = 5, set SYN flag to 1
    cli_segment.checksum = checksum(&cli_segment);

    //print segments
    printf("Sending SYN...\n");
    fprintf(f_log, "Sending SYN...\n");   //write to log
    segprint(&cli_segment, f_log);
    
    //send
    if(write(conn_fd, &cli_segment, seglen) == -1)
    {
        die("write()");
    }
    
    //prepare to receive ack from server
    memset((char *) &srv_segment, 0, seglen);  
    printf("Waiting..\n");
    if(read(conn_fd, &srv_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&srv_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    
    if((srv_segment.hdr_flags & 0x003F) != 0x0012) //check flags (last 6 bits);
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
    printf("Received SYNACK!\n");
    fprintf(f_log, "Received SYNACK!\n");//write to log
    segprint(&srv_segment, f_log);

    //prepare to send ACK for SYNACK
    //populate segment
    memset((char *) &cli_segment, 0, seglen);
    cli_segment.src_port = ntohs(si_client.sin_port);
    cli_segment.dest_port = portno;
    cli_segment.seq = srv_segment.ack;
    cli_segment.ack = srv_segment.seq + 1;
    cli_segment.hdr_flags = 0x5002; //header length = 5, set SYN flag to 1
    cli_segment.checksum = checksum(&cli_segment);

    //print segments
    printf("Sending ACK for SYNACK...\n");
    fprintf(f_log, "Sending ACK for SYNACK...\n");  //write to log
    segprint(&cli_segment, f_log);
    
    //send
    if(write(conn_fd, &cli_segment, seglen) == -1)
    {
        die("write()");
    }



    //close connection
    memset((char *) &cli_segment, 0, seglen);
    cli_segment.src_port = ntohs(si_client.sin_port);
    cli_segment.dest_port = portno;
    cli_segment.seq = 2046;
    cli_segment.ack = 1024;
    cli_segment.hdr_flags = 0x5001; //header length = 5, set SYN flag to 1
    cli_segment.checksum = checksum(&cli_segment);

    sleep(1);

    if(write(conn_fd, &cli_segment, seglen) == -1)
    {
        die("write()");
    }
    
    printf("Closing connection...\n");
    fprintf(f_log, "Closing connection...\n");
    segprint(&cli_segment, f_log);

    //wait for acknowledgement
    memset((char *) &srv_segment, 0, seglen);
    if(read(conn_fd, &srv_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&srv_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    if((srv_segment.hdr_flags & 0x003F) != 0x0002) //check flags (last 6 bits);
    {
        printf("Connection not established!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }

    printf("ACK for closure received!\n");
    fprintf(f_log, "ACK for closure received!\n");
    segprint(&srv_segment, f_log);

    //receive closure from server
    memset((char *) &srv_segment, 0, seglen);
    if(read(conn_fd, &srv_segment, seglen) == -1)
    {
        die("read()");
    }

    if(checksum(&srv_segment)) //checksum should be zero if computed and sent correctly
    {
        printf("Checksum mismatch!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }
    if((srv_segment.hdr_flags & 0x003F) != 0x0001) //check flags (last 6 bits);
    {
        printf("Connection not established!\n");
        printf("Output.txt not written\n");
        printf("Server log not written\n");
        fclose(f_write);
        fclose(f_log);
        close(conn_fd);
        return -1;
    }

    printf("Server closing connection\n");
    fprintf(f_log, "Server closing connection\n");
    segprint(&srv_segment, f_log);

    //close connection
    memset((char *) &cli_segment, 0, seglen);
    cli_segment.src_port = ntohs(si_client.sin_port);
    cli_segment.dest_port = portno;
    cli_segment.seq += 1;
    cli_segment.ack = srv_segment.seq + 1;
    cli_segment.hdr_flags = 0x5002; //header length = 5, set SYN flag to 1
    cli_segment.checksum = checksum(&cli_segment);

    sleep(1);

    if(write(conn_fd, &cli_segment, seglen) == -1)
    {
        die("write()");
    }
    
    printf("Acknowledging connection closure...\n");
    fprintf(f_log, "Acknowledging connection closure...\n");
    segprint(&cli_segment, f_log);

    close(conn_fd);
    fclose(f_log);
    close(cli_fd);

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