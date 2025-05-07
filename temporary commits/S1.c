// S1.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#define PORT 9001
#define BUFSIZE 1024

void prcclient(int client_fd) {
    char buffer[BUFSIZE];
    while (1) {
        memset(buffer, 0, BUFSIZE);
        int bytes = read(client_fd, buffer, BUFSIZE);
        if (bytes <= 0) break;

        printf("Received from client: %s\n", buffer);

        if (strncmp(buffer, "uploadf ", 8) == 0) {
            char filename[256], destpath[512];
            sscanf(buffer, "uploadf %s %s", filename, destpath);

            char *ext = strrchr(filename, '.');
            if (!ext) ext = "";

            char sizebuf[20] = {0};
            read(client_fd, sizebuf, sizeof(sizebuf));
            long fsize = atol(sizebuf);

            char fullpath[1024];
            if (strncmp(destpath, "~S1", 3) == 0)
                snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", destpath + 3);
            else
                snprintf(fullpath, sizeof(fullpath), "%s", destpath);

            char mkdir_cmd[1100];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", fullpath);
            system(mkdir_cmd);

            if (strcmp(ext, ".c") == 0) {
                char filepath[1100];
                snprintf(filepath, sizeof(filepath), "%s/%s", fullpath, filename);
                FILE *fp = fopen(filepath, "wb");
                if (!fp) {
                    write(client_fd, "Failed to create file", 22);
                    return;
                }

                long received = 0;
                char filebuf[BUFSIZE];
                while (received < fsize) {
                    int n = read(client_fd, filebuf, BUFSIZE);
                    fwrite(filebuf, 1, n, fp);
                    received += n;
                }
                fclose(fp);
                write(client_fd, "File stored in S1", 18);
                continue;
            } else {
                int target_port = 0;
                char server_path_prefix[256];

                if (strcmp(ext, ".pdf") == 0) {
                    target_port = 9002;
                    strcpy(server_path_prefix, "/home/vyas46/S2");
                } else if (strcmp(ext, ".txt") == 0) {
                    target_port = 9003;
                    strcpy(server_path_prefix, "/home/vyas46/S3");
                } else if (strcmp(ext, ".zip") == 0) {
                    target_port = 9004;
                    strcpy(server_path_prefix, "/home/vyas46/S4");
                } else {
                    write(client_fd, "Unsupported file type", 22);
                    continue;
                }

                int sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(target_port);
                inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
                connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

                send(sock, filename, strlen(filename), 0);
                usleep(100000);

                char actual_path[1024];
                snprintf(actual_path, sizeof(actual_path), "%s%s", server_path_prefix, destpath + 3);
                send(sock, actual_path, strlen(actual_path), 0);
                usleep(100000);

                char fsize_str[20];
                sprintf(fsize_str, "%ld", fsize);
                send(sock, fsize_str, strlen(fsize_str), 0);
                usleep(100000);

                long remaining = fsize;
                char filebuf[BUFSIZE];
                while (remaining > 0) {
                    int n = read(client_fd, filebuf, BUFSIZE);
                    send(sock, filebuf, n, 0);
                    remaining -= n;
                }

                read(sock, filebuf, BUFSIZE);
                write(client_fd, filebuf, strlen(filebuf));
                close(sock);
                continue;
            }
        }// emd of uploadf

        else if (strncmp(buffer, "downlf ", 7) == 0) {
            char filepath[512];
            sscanf(buffer, "downlf %s", filepath);

            char *ext = strrchr(filepath, '.');
            if (!ext) {
                write(client_fd, "0", 1); // invalid extension
                continue;
            }

            char fullpath[1024];
            if (strncmp(filepath, "~S1", 3) == 0)
                snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", filepath + 3);
            else
                snprintf(fullpath, sizeof(fullpath), "%s", filepath);

            if (strcmp(ext, ".c") == 0) {
                FILE *fp = fopen(fullpath, "rb");
                if (!fp) {
                    write(client_fd, "0", 1);
                    continue;
                }

                fseek(fp, 0, SEEK_END);
                long fsize = ftell(fp);
                rewind(fp);
                char fsize_str[20];
                sprintf(fsize_str, "%ld", fsize);
                write(client_fd, fsize_str, strlen(fsize_str));
                usleep(100000);

                char buf[BUFSIZE];
                size_t n;
                while ((n = fread(buf, 1, BUFSIZE, fp)) > 0) {
                    write(client_fd, buf, n);
                }
                fclose(fp);
                continue;
            }

		else {
                int target_port = 0;
                if (strcmp(ext, ".pdf") == 0) target_port = 9002;
                else if (strcmp(ext, ".txt") == 0) target_port = 9003;
                else if (strcmp(ext, ".zip") == 0) target_port = 9004;
                else {
                    write(client_fd, "0", 1);
                    continue;
                }

                int sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(target_port);
                inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    perror("Failed to connect to backend");
                    write(client_fd, "0", 1);
                    close(sock);
                    continue;
                }

		char remote_path[1024];
		if (strcmp(ext, ".pdf") == 0)
		    snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S2/%s", filepath + 4); // replace ~S1
		else if (strcmp(ext, ".txt") == 0)
		    snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S3/%s", filepath + 4);
		else if (strcmp(ext, ".zip") == 0)
		    snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S4/%s", filepath + 4);

		write(sock, remote_path, strlen(remote_path));

                //write(sock, fullpath, strlen(fullpath));
                usleep(100000);

                char sizebuf[20];
                int n = read(sock, sizebuf, sizeof(sizebuf));
                sizebuf[n] = '\0';
                long fsize = atol(sizebuf);
                if (fsize <= 0) {
                    write(client_fd, "0", 1);
                    close(sock);
                    continue;
                }

                write(client_fd, sizebuf, strlen(sizebuf));
                usleep(100000);

                char buf[BUFSIZE];
                long received = 0;
                while (received < fsize) {
                    int bytes = read(sock, buf, BUFSIZE);
                    if (bytes <= 0) break;
                    write(client_fd, buf, bytes);
                    received += bytes;
                }
                close(sock);
                continue;
            }
        }// emd of downlf

	//remove file logic
        else if (strncmp(buffer, "removef ", 8) == 0) {
            char filepath[512];
            sscanf(buffer, "removef %s", filepath);

            char *ext = strrchr(filepath, '.');
            if (!ext) {
                write(client_fd, "Unsupported or missing file extension\n", 39);
                continue;
            }

            char fullpath[1024];
            if (strncmp(filepath, "~S1", 3) == 0)
                snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", filepath + 3);
            else
                snprintf(fullpath, sizeof(fullpath), "%s", filepath);

            if (strcmp(ext, ".c") == 0) {
                if (remove(fullpath) == 0)
                    write(client_fd, "File deleted from S1\n", 22);
                else
                    write(client_fd, "File not found in S1\n", 22);
                continue;
            }

            // Forward to S2/S3/S4
            int target_port = 0;
            if (strcmp(ext, ".pdf") == 0) target_port = 9002;
            else if (strcmp(ext, ".txt") == 0) target_port = 9003;
            else if (strcmp(ext, ".zip") == 0) target_port = 9004;
            else {
                write(client_fd, "Unsupported file type\n", 23);
                continue;
            }

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(target_port);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection to backend server failed");
                write(client_fd, "Failed to connect to backend\n", 30);
                close(sock);
                continue;
            }

            // Adjust path before sending (S1→S2)
            char remote_path[1024];
            if (strcmp(ext, ".pdf") == 0)
                snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S2%s", filepath + 4);
            else if (strcmp(ext, ".txt") == 0)
                snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S3%s", filepath + 4);
            else if (strcmp(ext, ".zip") == 0)
                snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S4%s", filepath + 4);

            write(sock, remote_path, strlen(remote_path));
            usleep(100000);

            char reply[BUFSIZE];
            int n = read(sock, reply, BUFSIZE);
            if (n > 0) {
                reply[n] = '\0';
                write(client_fd, reply, strlen(reply));
            } else {
                write(client_fd, "No response from backend\n", 26);
            }
            close(sock);
            continue;
        }// end of removef



        else {
            write(client_fd, "OK from S1", strlen("OK from S1"));
        }
    }
    close(client_fd);
}


int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);
    printf("S1 Server listening on port %d...\n", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (fork() == 0) {
            close(server_fd);
            prcclient(client_fd);
            exit(0);
        }


	
        close(client_fd);
    }//end of while

}//end of main
