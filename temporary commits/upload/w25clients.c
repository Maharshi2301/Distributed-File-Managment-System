// w25clients.c (Fixed)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 9001
#define BUFSIZE 1024

int main() {
    struct sockaddr_in serv_addr;
    char buffer[BUFSIZE], input[BUFSIZE];

    while (1) {
        printf("w25clients$ ");
        fgets(input, BUFSIZE, stdin);
        input[strcspn(input, "\n")] = 0; // remove newline

        if (strcmp(input, "exit") == 0) break;

        // Open a new socket for each command
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            perror("Connection to server failed");
            close(sock);
            continue;
        }

        // Handle uploadf command separately
        if (strncmp(input, "uploadf ", 8) == 0) {
            char filename[256], destpath[512];
            if (sscanf(input, "uploadf %s %s", filename, destpath) != 2) {
                printf("Invalid syntax. Use: uploadf <filename> <dest_path>\n");
                close(sock);
                continue;
            }

            FILE *fp = fopen(filename, "rb");
            if (!fp) {
                perror("File open failed");
                close(sock);
                continue;
            }

            // Send command
            send(sock, input, strlen(input), 0);
            usleep(100000);

            // Send file size
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            rewind(fp);
            char fsize_str[20];
            sprintf(fsize_str, "%ld", fsize);
            send(sock, fsize_str, sizeof(fsize_str), 0);
            usleep(100000);

            // Send file content
            size_t n;
            while ((n = fread(buffer, 1, BUFSIZE, fp)) > 0) {
                send(sock, buffer, n, 0);
            }
            fclose(fp);

            // Read server response
            memset(buffer, 0, BUFSIZE);
            int bytes = read(sock, buffer, BUFSIZE);
            if (bytes <= 0) {
                perror("Read failed or server disconnected");
                close(sock);
                continue;
            }
            buffer[bytes] = '\0';
            printf("S1: %s\n", buffer);

            close(sock);
            continue;
        }

        // For other commands (not handled here yet)
        send(sock, input, strlen(input), 0);
        memset(buffer, 0, BUFSIZE);
        int bytes = read(sock, buffer, BUFSIZE);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("S1 response: %s\n", buffer);
        } else {
            perror("Read failed or server disconnected");
        }

        close(sock);
    }

    return 0;
}
