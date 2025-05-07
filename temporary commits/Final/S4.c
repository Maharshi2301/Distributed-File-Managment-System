// S4.c – Handles uploadf, downlf, removef, and dispfnames, tarfiles for .zip files 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 22123
#define BUFSIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    char buffer[BUFSIZE];
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // create server socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // accept connections on any local IP
    address.sin_port = htons(PORT); // set port for s4

    bind(server_fd, (struct sockaddr *)&address, sizeof(address)); // bind the socket
    listen(server_fd, 3); // accept up to 3 pending connections
    printf("S4 ready on port %d...\n", PORT); // show that s4 started

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen); // wait for client
        printf("Connection received in S4.\n"); // log new client

        // Peek to detect command type
        char peekbuf[BUFSIZE] = {0};
        int peek_n = recv(client_fd, peekbuf, BUFSIZE, MSG_PEEK); // just check what's coming
        if (peek_n <= 0) {
            perror("Initial peek failed"); // nothing came or something broke
            close(client_fd);
            continue;
        }

        // ✅ dispfnames
        if (strncmp(peekbuf, "dispfnames ", 11) == 0) { // check if client wants file list
            memset(buffer, 0, BUFSIZE);
            int n = read(client_fd, buffer, BUFSIZE); // read command
            if (n <= 0) {
                perror("Failed to read dispfnames command"); // reading failed
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // null-terminate it

            char path[512];
            sscanf(buffer, "dispfnames %s", path); // extract path from cmd
            printf("📂 dispfnames request for: %s\n", path); // log request

            char listcmd[1100];
            snprintf(listcmd, sizeof(listcmd), "ls -p \"%s\" 2>/dev/null | grep '\\.zip$' | sort", path); // list only .zip files

            FILE *fp = popen(listcmd, "r"); // run shell command
            if (!fp) {
                write(client_fd, "Failed to list files\n", 22); // tell client something went wrong
                close(client_fd);
                continue;
            }

            char filelist[BUFSIZE] = {0};
            fread(filelist, 1, sizeof(filelist), fp); // read the output
            pclose(fp); // done with command

            if (strlen(filelist) == 0)
                write(client_fd, "", 0); // nothing to send
            else
                write(client_fd, filelist, strlen(filelist)); // send the list

            close(client_fd); // end conn
            continue;
        }

        // ✅ removef (only log for .zip deletions)
        if (strncmp(peekbuf, "removef ", 8) == 0) { // intercept any zip delete attempt
            memset(buffer, 0, BUFSIZE);
            int n = read(client_fd, buffer, BUFSIZE); // read full removef cmd
            if (n <= 0) {
                perror("Failed to read removef command"); // nothing came
                close(client_fd);
                continue;
            }
            buffer[n] = '\0';
            char *filepath = buffer + 8; // skip the "removef " part

            // Print message only, no deletion
            printf("❌ You are not authorized to delete zip file: %s\n", filepath); // just show the msg

            // Close the connection silently
            close(client_fd); // do nothing else
            continue;
        }

            // ✅ downlf
            else if (strncmp(peekbuf, "/home/", 6) == 0) { // check if it’s a file download request
                memset(buffer, 0, BUFSIZE); // clear the buffer
                int n = read(client_fd, buffer, BUFSIZE); // read full file path
                if (n <= 0) {
                    perror("Failed to read downlf path"); // something went wrong
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0'; // null terminate
                printf("📥 downlf request: %s\n", buffer); // log the path we got

                FILE *fp = fopen(buffer, "rb"); // try to open file
                if (!fp) {
                    write(client_fd, "0", 1); // tell client we couldn't find the file
                    close(client_fd);
                    continue;
                }

                fseek(fp, 0, SEEK_END); // go to end of file
                long fsize = ftell(fp); // get file size
                rewind(fp); // back to start
                char sizebuf[20];
                sprintf(sizebuf, "%ld", fsize); // convert to string
                write(client_fd, sizebuf, strlen(sizebuf)); // send file size
                usleep(100000); // pause a bit before sending file

                long sent = 0;
                while (sent < fsize) { // send file in chunks
                    int count = fread(buffer, 1, BUFSIZE, fp); // read some data
                    write(client_fd, buffer, count); // send it
                    sent += count;
                }
                fclose(fp); // close file
                printf("✅ Sent file (%ld bytes)\n", fsize); // log sent status
                close(client_fd); // done with client
                continue;
            }

            // ✅ uploadf
            memset(buffer, 0, BUFSIZE); // reset buffer
            int n = read(client_fd, buffer, BUFSIZE); // read filename
            if (n <= 0) {
                perror("Filename read failed");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // end string
            char filename[256];
            strcpy(filename, buffer); // store filename
            printf("📤 Filename: %s\n", filename); // log name

            memset(buffer, 0, BUFSIZE);
            n = read(client_fd, buffer, BUFSIZE); // read destination folder
            if (n <= 0) {
                perror("Path read failed");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // terminate
            char destpath[512];
            strcpy(destpath, buffer); // store path
            printf("📁 Destination path: %s\n", destpath); // log path

            memset(buffer, 0, BUFSIZE);
            n = read(client_fd, buffer, BUFSIZE); // read file size
            if (n <= 0) {
                perror("File size read failed");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // string end
            long fsize = atol(buffer); // convert to long
            printf("📦 File size: %ld bytes\n", fsize); // log size

            char mkdir_cmd[1024];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", destpath); // make folder if needed
            system(mkdir_cmd); // run mkdir command

            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", destpath, filename); // build full file path
            FILE *fp = fopen(filepath, "wb"); // open for binary write
            if (!fp) {
                perror("fopen failed"); // error if file can't be created
                write(client_fd, "File write error\n", 18); // tell client
                close(client_fd);
                continue;
            }

            long received = 0;
            while (received < fsize) { // keep reading until full file is received
                n = read(client_fd, buffer, BUFSIZE); // get file chunk
                if (n <= 0) break; // if no data, stop
                fwrite(buffer, 1, n, fp); // write chunk to file
                received += n; // update counter
            }
            fclose(fp); // done with file

            printf("✅ Stored file: %s\n", filepath); // log save path
            write(client_fd, "File saved in S4\n", 18); // notify client
            close(client_fd); // done with this request
        }
    }
