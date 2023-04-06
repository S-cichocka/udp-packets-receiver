#include "sqlite/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#define INET_ADDRSTRLEN 16
#pragma comment(lib, "Ws2_32.lib")
#define MAX_BUFFER_SIZE 1500
int get_port(struct sockaddr* addr) {
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    return ntohs(addr_in->sin_port);
}

char* get_address_str(struct sockaddr* addr) {
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    char* buffer = malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(addr_in->sin_addr), buffer, INET_ADDRSTRLEN);
    return buffer;
}

int main(int argc, char* argv[]) {

    //INPUT PARAMETERS
    char *source_ip = argv[1];
    int port = atoi(argv[2]);
    char** errmsg;
    struct sockaddr_in addrvalid;
    int ip_valid = inet_pton(AF_INET, source_ip, &(addrvalid.sin_addr));
    if (ip_valid <= 0) {
        if (ip_valid == 0)
            printf("Invalid IP address: %s\n", source_ip);
        else
            perror("inet_pton() error");
        exit(EXIT_FAILURE);
    }
    if (port <= 0 || port > 65535) {
        printf("Invalid port number: %d\n", port);
        exit(EXIT_FAILURE);
    }
#ifdef WIN32
    // INITIALIZE WINSOCK
    int iResult;
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed: %d", iResult);
        return;
    }
#endif
#ifdef WIN32
    typedef int socklen_t;
#endif

    //DB OPEN
    sqlite3* db;
    int rc = sqlite3_open("udp_packets.db", &db);

    if (rc != SQLITE_OK) {
        printf("FILE DIDN'T OPEN \n");
    }
    else
        printf("FILE OPENED \n");
    //SOCKET CREATION
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        perror("SOCKET ERROR");
        exit(EXIT_FAILURE);
    }
    //SET IP & PORT FOR SOCKET
    struct sockaddr_in sockaddr = {0};
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    inet_pton(AF_INET, source_ip, &sockaddr.sin_addr);

    if (bind(sockfd, (struct sockaddr_in*)&sockaddr, sizeof(sockaddr)) < 0) {
        perror("BIND ERROR");
        exit(EXIT_FAILURE); 
    }
    //UDP PACKET RECEIVING LOOP
    while (1)
    {
        struct sockaddr_storage sockrec;
        socklen_t sockrec_len = sizeof(sockrec);
        char buffer[MAX_BUFFER_SIZE];
        int pckt_size = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr*)&sockrec, &sockrec_len);
        // CHECKING IF WE HAVE RECEIVED A PACKAGE
        if (pckt_size < 0) {
            perror("RECEIVE ERROR");
            continue;
        }
        //DISPLAY UDP INFORMATION IN THE CONSOLE
        printf("Received UDP packet from %s:%d\n", get_address_str((struct sockaddr*)&sockrec), get_port((struct sockaddr*)&sockrec));
        printf("Packet size: %d\n", pckt_size);

        //SAVE RECORD INTO DB TIMESTAMP
        time_t saved_timestamp = time(NULL);
        //CREATING QUERY FOR DB
        char* que_sql = "INSERT INTO packets (timestamp, source_ip, size_of_packet, saved_timestamp) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, que_sql, strlen(que_sql) + 1, &stmt, NULL);
        //CHECKING STATEMENT ERROR
        if (rc != SQLITE_OK) {

            printf("SQL STATEMENT ERROR");
            sqlite3_close(db);

            return 1;
        }
        sqlite3_bind_int64(stmt, 1, time(NULL)); // UDP RECEIVE TIMESTAMP ADD
        sqlite3_bind_text(stmt, 2, get_address_str((struct sockaddr*)&sockrec), strlen(get_address_str((struct sockaddr*)&sockrec)), SQLITE_STATIC); // SOURCE IP ADD
        sqlite3_bind_int(stmt, 3, pckt_size); // PACKET SIZE ADD
        sqlite3_bind_int64(stmt, 4, saved_timestamp); // RECORD TIMESTAMP ADD
        //ADDED RECORD TO DB
        //EXECUTING DB QUERY
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            printf("EXECUTING QUERY ERROR");
            continue;
        }
        printf("DATA INSERTED SUCCESSFULLY\n");
        //CLOSING SQL QUERY
        sqlite3_finalize(stmt);
    }
    //CLOSING THE CONNECTION WITH DB AND SOCKET
    sqlite3_close(db);
    close(sockfd);
    return 0;
}