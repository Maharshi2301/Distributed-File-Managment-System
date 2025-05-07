// S2.c – Handles uploadf and downlf for .pdf files

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 9002
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
    printf("S2 ready on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        printf("Connection received in S2.\n");

        // Peek to see if it is a downlf command
        char peekbuf[BUFSIZE] = {0};
        int peek_n = recv(client_fd, peekbuf, BUFSIZE, MSG_PEEK);
        if (peek_n <= 0) {
            perror("Initial peek failed");
            close(client_fd);
            continue;
        }

        // If first data starts with a file path (downlf)
        if (strncmp(peekbuf, "/home/", 6) == 0) {
            // Handle downlf
            memset(buffer, 0, BUFSIZE);
            int n = read(client_fd, buffer, BUFSIZE);
            if (n <= 0) {
                perror("Failed to read downlf path");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0';
            printf("downlf request: %s\n", buffer);

            FILE *fp = fopen(buffer, "rb");
            if (!fp) {
                write(client_fd, "0", 1);
                close(client_fd);
                continue;
            }

            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            rewind(fp);
            char sizebuf[20];
            sprintf(sizebuf, "%ld", fsize);
            write(client_fd, sizebuf, strlen(sizebuf));
            usleep(100000);

            long sent = 0;
            while (sent < fsize) {
                int count = fread(buffer, 1, BUFSIZE, fp);
                write(client_fd, buffer, count);
                sent += count;
            }
            fclose(fp);
            printf("✅ Sent file (%ld bytes)\n", fsize);
            close(client_fd);
            continue;
        }

        // Otherwise handle uploadf
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
        write(client_fd, "File saved in S2\n", 18);
        close(client_fd);
    }
}
