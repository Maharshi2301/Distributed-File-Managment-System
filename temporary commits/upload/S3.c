// S3.c – Receives .txt files from S1 (Stable Version)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 9003
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
    printf("S3 ready on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        printf("Connection received in S3.\n");

        // 1. Receive filename
        memset(buffer, 0, BUFSIZE);
        int n = read(client_fd, buffer, BUFSIZE);
        if (n <= 0) {
            perror("Filename read failed");
            close(client_fd);
            continue;
        }
        buffer[n] = '\0';
        char filename[256];
        strcpy(filename, buffer);
        printf("Filename: %s\n", filename);

        // 2. Receive destination path
        memset(buffer, 0, BUFSIZE);
        n = read(client_fd, buffer, BUFSIZE);
        if (n <= 0) {
            perror("Path read failed");
            close(client_fd);
            continue;
        }
        buffer[n] = '\0';
        char destpath[512];
        strcpy(destpath, buffer);
        printf("Destination path: %s\n", destpath);

        // 3. Receive file size
        memset(buffer, 0, BUFSIZE);
        n = read(client_fd, buffer, BUFSIZE);
        if (n <= 0) {
            perror("File size read failed");
            close(client_fd);
            continue;
        }
        buffer[n] = '\0';
        long fsize = atol(buffer);
        printf("File size: %ld bytes\n", fsize);

        // 4. Create directory
        char mkdir_cmd[1024];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", destpath);
        system(mkdir_cmd);

        // 5. Receive file content
        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s", destpath, filename);
        FILE *fp = fopen(filepath, "wb");
        if (!fp) {
            perror("fopen failed");
            write(client_fd, "File write error\n", 18);
            close(client_fd);
            continue;
        }

        long received = 0;
        while (received < fsize) {
            n = read(client_fd, buffer, BUFSIZE);
            if (n <= 0) break;
            fwrite(buffer, 1, n, fp);
            received += n;
        }
        fclose(fp);

        printf("✅ Stored file: %s\n", filepath);
        write(client_fd, "File saved in S3\n", 18);
        close(client_fd);
    }
}
