// S3.c – Handles uploadf, downlf, removef, and dispfnames, tarfiles for .txt files

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 21777
#define BUFSIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    char buffer[BUFSIZE];
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // create server socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // accept connections on any IP
    address.sin_port = htons(PORT); // port 21777 for s3

    bind(server_fd, (struct sockaddr *)&address, sizeof(address)); // bind socket
    listen(server_fd, 3); // allow up to 3 queued connections
    printf("S3 ready on port %d...\n", PORT); // log server start

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen); // wait for new client
        printf("Connection received in S3.\n");

        // Peek to detect command type
        char peekbuf[BUFSIZE] = {0}; // peek buffer
        int peek_n = recv(client_fd, peekbuf, BUFSIZE, MSG_PEEK); // preview client message
        if (peek_n <= 0) {
            perror("Initial peek failed"); // something went wrong
            close(client_fd);
            continue;
        }

        // ✅ dispfnames
        if (strncmp(peekbuf, "dispfnames ", 11) == 0) { // check if command is dispfnames
            memset(buffer, 0, BUFSIZE); // clear buffer
            int n = read(client_fd, buffer, BUFSIZE); // read full command
            if (n <= 0) {
                perror("Failed to read dispfnames command"); // log fail
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // terminate string

            char path[512];
            sscanf(buffer, "dispfnames %s", path); // pull path out of cmd
            printf("📂 dispfnames request for: %s\n", path); // log request

            char listcmd[1100];
            snprintf(listcmd, sizeof(listcmd), "ls -p \"%s\" 2>/dev/null | grep '\\.txt$' | sort", path); // list only .txt files

            FILE *fp = popen(listcmd, "r"); // run shell cmd to get files
            if (!fp) {
                write(client_fd, "Failed to list files\n", 22); // tell client it failed
                close(client_fd);
                continue;
            }

            char filelist[BUFSIZE] = {0}; // buffer to hold list of files
            fread(filelist, 1, sizeof(filelist), fp); // read command output
            pclose(fp); // done with shell process

            if (strlen(filelist) == 0)
                write(client_fd, "", 0); // no files found, send empty
            else
                write(client_fd, filelist, strlen(filelist)); // send list back to client

            close(client_fd); // done with connection
            continue;
        }

            // ✅ removef
            if (strncmp(peekbuf, "removef ", 8) == 0) { // check if it's a file delete request
                memset(buffer, 0, BUFSIZE); // clear buffer
                int n = read(client_fd, buffer, BUFSIZE); // read the full removef command
                if (n <= 0) {
                    perror("Failed to read removef command"); // nothing read
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0'; // terminate string
                char *filepath = buffer + 8; // skip "removef " part
                printf("🗑️ File operation request: %s\n", filepath); // log the file path

                if (remove(filepath) == 0) { // try deleting the file
                    printf("✅ File deleted: %s\n", filepath); // success
                    write(client_fd, "File deleted from S1\n", 22); // notify client
                } else {
                    perror("Delete failed"); // file didn't exist or couldn't be removed
                    write(client_fd, "File not found in S1\n", 22); // let client know
                }
                close(client_fd); // done
                continue;
            }

            // ✅ downltar
            if (strncmp(peekbuf, "downltar ", 9) == 0) { // check for tar download
                memset(buffer, 0, BUFSIZE); // clear buffer
                read(client_fd, buffer, BUFSIZE); // read command (even if not used here)
                char tarname[32], ext[10], path[256];

                strcpy(tarname, "text.tar"); // output tar name
                strcpy(ext, "*.txt"); // file type to bundle
                strcpy(path, "/home/vyas46/S3"); // folder to look in

                char cmd[512];
                snprintf(cmd, sizeof(cmd), "cd %s && tar -cf %s $(find . -name \"%s\")", path, tarname, ext); // build tar command
                system(cmd); // run it

                char tarpath[512];
                snprintf(tarpath, sizeof(tarpath), "%s/%s", path, tarname); // get full tar path
                FILE *fp = fopen(tarpath, "rb"); // open tar file
                if (!fp) {
                    write(client_fd, "0", 1); // send fail
                    close(client_fd);
                    continue;
                }

                // send tar size first
                fseek(fp, 0, SEEK_END);
                long fsize = ftell(fp); // get size
                rewind(fp);
                char sizebuf[20];
                sprintf(sizebuf, "%ld", fsize); // to string
                write(client_fd, sizebuf, strlen(sizebuf)); // send size
                usleep(100000); // wait a bit

                // send file content chunk by chunk
                size_t n;
                while ((n = fread(buffer, 1, BUFSIZE, fp)) > 0)
                    write(client_fd, buffer, n);
                fclose(fp);
                close(client_fd); // close after sending
                continue;
            }

            // ✅ downlf
            else if (strncmp(peekbuf, "/home/", 6) == 0) { // if it’s a normal file download request
                memset(buffer, 0, BUFSIZE); // clear buffer
                int n = read(client_fd, buffer, BUFSIZE); // read full path
                if (n <= 0) {
                    perror("Failed to read downlf path");
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0'; // end string
                printf("📥 downlf request: %s\n", buffer); // log file request

                FILE *fp = fopen(buffer, "rb"); // try to open file
                if (!fp) {
                    write(client_fd, "0", 1); // tell client it failed
                    close(client_fd);
                    continue;
                }

                fseek(fp, 0, SEEK_END); // go to end of file
                long fsize = ftell(fp); // get file size
                rewind(fp); // go back to start
                char sizebuf[20];
                sprintf(sizebuf, "%ld", fsize); // convert size to str
                write(client_fd, sizebuf, strlen(sizebuf)); // send size to client
                usleep(100000); // wait a bit

                long sent = 0;
                while (sent < fsize) {
                    int count = fread(buffer, 1, BUFSIZE, fp); // read next chunk
                    write(client_fd, buffer, count); // send chunk
                    sent += count;
                }
                fclose(fp); // close file
                printf("✅ Sent file (%ld bytes)\n", fsize); // log sent size
                close(client_fd); // close socket
                continue;
            }

            // ✅ uploadf
            memset(buffer, 0, BUFSIZE); // clear the buffer
            int n = read(client_fd, buffer, BUFSIZE); // read file name from client
            if (n <= 0) {
                perror("Filename read failed"); // handle read error
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // null terminate
            char filename[256];
            strcpy(filename, buffer); // save filename
            printf("📤 Filename: %s\n", filename); // log it

            memset(buffer, 0, BUFSIZE); // clear buffer again
            n = read(client_fd, buffer, BUFSIZE); // read path from client
            if (n <= 0) {
                perror("Path read failed"); // handle read fail
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // null terminate
            char destpath[512];
            strcpy(destpath, buffer); // save the path
            printf("📁 Destination path: %s\n", destpath); // show path

            memset(buffer, 0, BUFSIZE); // clear before next read
            n = read(client_fd, buffer, BUFSIZE); // read file size
            if (n <= 0) {
                perror("File size read failed"); // file size missing
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // end string
            long fsize = atol(buffer); // convert to long
            printf("📦 File size: %ld bytes\n", fsize); // print size

            char mkdir_cmd[1024];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", destpath); // build mkdir command
            system(mkdir_cmd); // make sure path exists

            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", destpath, filename); // full path to save file
            FILE *fp = fopen(filepath, "wb"); // open file to write
            if (!fp) {
                perror("fopen failed"); // error opening
                write(client_fd, "File write error\n", 18); // tell client
                close(client_fd);
                continue;
            }

            long received = 0;
            while (received < fsize) { // keep reading until full file is received
                n = read(client_fd, buffer, BUFSIZE); // read chunk
                if (n <= 0) break; // stop if nothing left
                fwrite(buffer, 1, n, fp); // write chunk to file
                received += n; // update received size
            }
            fclose(fp); // close file after writing

            printf("✅ Stored file: %s\n", filepath); // log where file is stored
            write(client_fd, "File saved in S1\n", 18); // notify client
            close(client_fd); // close socket
        }
    }


