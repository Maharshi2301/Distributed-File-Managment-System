// S4.c – Handles uploadf, downlf, removef, and dispfnames for .zip files

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

            // Peek to detect command type
            char peekbuf[BUFSIZE] = {0};
            int peek_n = recv(client_fd, peekbuf, BUFSIZE, MSG_PEEK);
            if (peek_n <= 0) {
                perror("Initial peek failed");
                close(client_fd);
                continue;
            }

            // ✅ dispfnames
            if (strncmp(peekbuf, "dispfnames ", 11) == 0) {
                memset(buffer, 0, BUFSIZE);
                int n = read(client_fd, buffer, BUFSIZE);
                if (n <= 0) {
                    perror("Failed to read dispfnames command");
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0';

                char path[512];
                sscanf(buffer, "dispfnames %s", path);
                printf("📂 dispfnames request for: %s\n", path);

                char listcmd[1100];
                snprintf(listcmd, sizeof(listcmd), "ls -p \"%s\" 2>/dev/null | grep '\\.zip$' | sort", path);

                FILE *fp = popen(listcmd, "r");
                if (!fp) {
                    write(client_fd, "Failed to list files\n", 22);
                    close(client_fd);
                    continue;
                }

                char filelist[BUFSIZE] = {0};
                fread(filelist, 1, sizeof(filelist), fp);
                pclose(fp);

                if (strlen(filelist) == 0)
                    write(client_fd, "", 0);
                else
                    write(client_fd, filelist, strlen(filelist));

                close(client_fd);
                continue;
            }

                    // ✅ removef (only log for .zip deletions)
            if (strncmp(peekbuf, "removef ", 8) == 0) {
                memset(buffer, 0, BUFSIZE);
                int n = read(client_fd, buffer, BUFSIZE);
                if (n <= 0) {
                    perror("Failed to read removef command");
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0';
                char *filepath = buffer + 8;
                
                // Print message only, no deletion
                printf("❌ You are not authorized to delete zip file: %s\n", filepath);

                // Close the connection silently
                close(client_fd);
                continue;
            }


            // ✅ downlf
            else if (strncmp(peekbuf, "/home/", 6) == 0) {
                memset(buffer, 0, BUFSIZE);
                int n = read(client_fd, buffer, BUFSIZE);
                if (n <= 0) {
                    perror("Failed to read downlf path");
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0';
                printf("📥 downlf request: %s\n", buffer);

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

            // ✅ uploadf
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
            printf("📤 Filename: %s\n", filename);

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
            printf("📁 Destination path: %s\n", destpath);

            memset(buffer, 0, BUFSIZE);
            n = read(client_fd, buffer, BUFSIZE);
            if (n <= 0) {
                perror("File size read failed");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0';
            long fsize = atol(buffer);
            printf("📦 File size: %ld bytes\n", fsize);

            char mkdir_cmd[1024];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", destpath);
            system(mkdir_cmd);

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
            write(client_fd, "File saved in S4\n", 18);
            close(client_fd);
        }
    }
