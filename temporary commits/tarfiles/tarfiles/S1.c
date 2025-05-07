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

char *get_remote_list(const char *remote_path, int port);

void prcclient(int client_fd) {
    char buffer[BUFSIZE];
    while (1) {
        memset(buffer, 0, BUFSIZE);
        int bytes = read(client_fd, buffer, BUFSIZE);
        if (bytes <= 0) break;

        printf("Received from client: %s\n", buffer);
	fflush(stdout);
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
        }
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
            } else {
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
        }// end of downlf

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
                snprintf(remote_path, sizeof(remote_path), "removef /home/vyas46/S2/%s", filepath + 4);
            else if (strcmp(ext, ".txt") == 0)
                snprintf(remote_path, sizeof(remote_path), "removef /home/vyas46/S3/%s", filepath + 4);
            else if (strcmp(ext, ".zip") == 0)
                snprintf(remote_path, sizeof(remote_path), "removef /home/vyas46/S4/%s", filepath + 4);

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
        }//end of remove file


        else if (strncmp(buffer, "dispfnames ", 11) == 0) {
            char user_path[512];
            sscanf(buffer, "dispfnames %s", user_path);

            // Extract relative path from ~S1
            char *subpath = user_path + 3; // skip ~S1
            char full_local[512], full_s2[512], full_s3[512], full_s4[512];
            snprintf(full_local, sizeof(full_local), "/home/vyas46/S1%s", subpath);
            snprintf(full_s2, sizeof(full_s2), "/home/vyas46/S2%s", subpath);
            snprintf(full_s3, sizeof(full_s3), "/home/vyas46/S3%s", subpath);
            snprintf(full_s4, sizeof(full_s4), "/home/vyas46/S4%s", subpath);

            // Step 1: get .c files locally
            char cmd_c[600];
            snprintf(cmd_c, sizeof(cmd_c), "ls -p \"%s\" 2>/dev/null | grep '\\.c$' | sort", full_local);
            FILE *fp_c = popen(cmd_c, "r");
            char c_files[BUFSIZE] = {0};
            if (fp_c) {
                fread(c_files, 1, sizeof(c_files), fp_c);
                pclose(fp_c);
            }

            // Step 3: define list for all the files
            char combined[BUFSIZE * 2] = {0};
            strcat(combined, c_files);

	    // Step 4: get other types from S2, S3 and S4 and add it into the list
            char *pdf_files = get_remote_list(full_s2, 9002);
	    strcat(combined, pdf_files);
            char *txt_files = get_remote_list(full_s3, 9003);
	    strcat(combined, txt_files);
            char *zip_files = get_remote_list(full_s4, 9004);
	    strcat(combined, zip_files);

            // Step 5: send to client
            if (strlen(combined) == 0)
                write(client_fd, "No files found.\n", 16);
            else
                write(client_fd, combined, strlen(combined));

            continue;
        }// end of displfnames

        else if (strncmp(buffer, "downltar ", 9) == 0) {
                char filetype[10];
                sscanf(buffer, "downltar %s", filetype);

                char cmd[1024], tarname[32];
                int backend_port = 0;
                char *backend_path = NULL;

                if (strcmp(filetype, ".c") == 0) {
                        strcpy(tarname, "cfiles.tar");
                        snprintf(cmd, sizeof(cmd), "cd /home/vyas46/S1 && tar -cf %s $(find . -name \"*.c\")", tarname);
                        system(cmd);
                } else if (strcmp(filetype, ".pdf") == 0) {
                        backend_port = 9002;
                        strcpy(tarname, "pdf.tar");
                } else if (strcmp(filetype, ".txt") == 0) {
                        backend_port = 9003;
                        strcpy(tarname, "text.tar");
                } else {
                        write(client_fd, "Unsupported type\n", 18);
                        continue;
                }

                // If backend, forward request
                if (backend_port > 0) {
                        int sock = socket(AF_INET, SOCK_STREAM, 0);
                        struct sockaddr_in serv_addr;
                        serv_addr.sin_family = AF_INET;
                        serv_addr.sin_port = htons(backend_port);
                        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

                        connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        send(sock, buffer, strlen(buffer), 0); // send "downltar .pdf"
                        usleep(100000);

                        // Receive size
                        char sizebuf[20];
                        int n = read(sock, sizebuf, sizeof(sizebuf));
                        sizebuf[n] = '\0';
                        long fsize = atol(sizebuf);
                        write(client_fd, sizebuf, strlen(sizebuf));
                        usleep(100000);

                        // Relay tar file
                        long received = 0;
                        while (received < fsize) {
                                int n = read(sock, buffer, BUFSIZE);
                                write(client_fd, buffer, n);
                                received += n;
                        }
                        close(sock);
                        continue;
                }

                // Send file size
                char path[512];
                snprintf(path, sizeof(path), "/home/vyas46/S1/%s", tarname);
                FILE *fp = fopen(path, "rb");
                if (!fp) {
                        write(client_fd, "0", 1);
                        continue;
                }

                fseek(fp, 0, SEEK_END);
                long fsize = ftell(fp);
                rewind(fp);
                char sizebuf[20];
                sprintf(sizebuf, "%ld", fsize);
                write(client_fd, sizebuf, strlen(sizebuf));
                usleep(100000);

                size_t n;
                while ((n = fread(buffer, 1, BUFSIZE, fp)) > 0)
                        write(client_fd, buffer, n);
                fclose(fp);
                continue;
        }


        else {
            write(client_fd, "OK from S1", strlen("OK from S1"));
        }
    }
    close(client_fd);
}

// Step 2: define helper for backend calls
char *get_remote_list(const char *remote_path, int port) {
	static char output[BUFSIZE];
	memset(output, 0, BUFSIZE);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sock);
		return output;
	}

	char cmd[600];
	snprintf(cmd, sizeof(cmd), "dispfnames %s", remote_path);
	send(sock, cmd, strlen(cmd), 0);
	usleep(100000);

	read(sock, output, BUFSIZE);
	close(sock);
	return output;
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
	fflush(stdout);
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
