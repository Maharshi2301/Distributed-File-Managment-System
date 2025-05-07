// S4.c – Receives .pdf files from S1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 9004
#define BUFSIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    char buffer[BUFSIZE];
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    printf("S4 ready on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        printf("Connection received in S4.\n");

        // Receive filename
        memset(buffer, 0, BUFSIZE);
        read(client_fd, buffer, BUFSIZE);
        char filename[256];
        strcpy(filename, buffer);

        // Receive path
        memset(buffer, 0, BUFSIZE);
        read(client_fd, buffer, BUFSIZE);
        char destpath[512];
        strcpy(destpath, buffer);

        // Receive file size
        memset(buffer, 0, BUFSIZE);
        read(client_fd, buffer, BUFSIZE);
        long fsize = atol(buffer);

        // Create directory
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", destpath);
        system(mkdir_cmd);

        // Save file
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", destpath, filename);
        FILE *fp = fopen(filepath, "wb");
        long received = 0;
        while (received < fsize) {
            int n = read(client_fd, buffer, BUFSIZE);
            fwrite(buffer, 1, n, fp);
            received += n;
        }
        fclose(fp);

        write(client_fd, "File saved in S4\n", 18);
        close(client_fd);
    }
}
