// w25clients.c (Fixed)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 19456
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
        }//end of uploadf



	//downlf logic
        if (strncmp(input, "downlf ", 7) == 0) {
            // Create a new socket
            sock = socket(AF_INET, SOCK_STREAM, 0);
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

            // Send the downlf command
            send(sock, input, strlen(input), 0);
            usleep(100000);

            // Receive file size
            char sizebuf[20];
            int n = read(sock, sizebuf, sizeof(sizebuf));
            if (n <= 0) {
                perror("Failed to receive file size");
                close(sock);
                continue;
            }

            sizebuf[n] = '\0';
            long fsize = atol(sizebuf);
            if (fsize <= 0) {
                printf("File not found or error fetching file.\n");
                close(sock);
                continue;
            }

            // Extract filename from input path
            char *filepath = input + 7; // skip "downlf "
            char *filename = strrchr(filepath, '/');
            if (!filename) filename = filepath;
            else filename++; // move past '/'

            FILE *fp = fopen(filename, "wb");
            if (!fp) {
                perror("Failed to create file locally");
                close(sock);
                continue;
            }

            long received = 0;
            while (received < fsize) {
                n = read(sock, buffer, BUFSIZE);
                if (n <= 0) break;
                fwrite(buffer, 1, n, fp);
                received += n;
            }
            fclose(fp);

            printf("✅ File '%s' downloaded successfully (%ld bytes)\n", filename, fsize);
            close(sock);
            continue;
        }//end of downlf


        else if (strncmp(input, "removef ", 8) == 0) {
            // Create new socket
            sock = socket(AF_INET, SOCK_STREAM, 0);
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

            // Send the command
            send(sock, input, strlen(input), 0);
            usleep(100000);

            // Read response
            memset(buffer, 0, BUFSIZE);
            int bytes = read(sock, buffer, BUFSIZE);
            if (bytes > 0) {
                buffer[bytes] = '\0';

                // Check if server response indicates failure
                if (strstr(buffer, "not found") || strstr(buffer, "Not found")) {
                    printf("File not found or error fetching file.\n");
                } else {
                    printf("S1: %s\n", buffer);
                }
            } else {
                perror("Failed to receive response");
            }

            close(sock);
            continue;
        }// end of remove file

        else if (strncmp(input, "dispfnames ", 11) == 0) {
            // Create socket
            sock = socket(AF_INET, SOCK_STREAM, 0);
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

            // Send command to S1
            send(sock, input, strlen(input), 0);
            usleep(100000);

            // Read response
            memset(buffer, 0, BUFSIZE);
            int bytes = read(sock, buffer, BUFSIZE);
            if (bytes > 0) {
                buffer[bytes] = '\0';

                if (strstr(buffer, "No files found") || strstr(buffer, "Failed to list files"))
                    printf("❌ %s\n", buffer);
                else {
                    printf("📂 Files in directory (grouped by type):\n%s\n", buffer);
                }
            } else {
                printf("❌ Failed to receive file list.\n");
            }

            close(sock);
            continue;
        }//end of dispfnames


        else if (strncmp(input, "downltar ", 9) == 0) {
            char filetype[10];
            sscanf(input, "downltar %s", filetype);

            char tarname[32];
            if (strcmp(filetype, ".c") == 0) strcpy(tarname, "cfiles.tar");
            else if (strcmp(filetype, ".pdf") == 0) strcpy(tarname, "pdf.tar");
            else if (strcmp(filetype, ".txt") == 0) strcpy(tarname, "text.tar");
            else {
                printf("Unsupported filetype for downltar\n");
                close(sock);
                continue;
            }

            send(sock, input, strlen(input), 0);
            usleep(100000);

            char sizebuf[20];
            int n = read(sock, sizebuf, sizeof(sizebuf));
            if (n <= 0) {
                printf("Failed to get size\n");
                close(sock);
                continue;
            }
            sizebuf[n] = '\0';
            long fsize = atol(sizebuf);
            if (fsize <= 0) {
                printf("No such files to archive\n");
                close(sock);
                continue;
            }

            FILE *fp = fopen(tarname, "wb");
            if (!fp) {
                perror("Failed to create tar file locally");
                close(sock);
                continue;
            }

            long received = 0;
            while (received < fsize) {
                n = read(sock, buffer, BUFSIZE);
                fwrite(buffer, 1, n, fp);
                received += n;
            }
            fclose(fp);
            printf("✅ Tarball '%s' downloaded (%ld bytes)\n", tarname, fsize);
            close(sock);
            continue;
        } // End of downltar function



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
