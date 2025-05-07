// S2.c – Handles uploadf, downlf, removef, and dispfnames, tarfiles for .pdf files 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 21333
#define BUFSIZE 1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    char buffer[BUFSIZE];
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // make a socket for server
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // listen on any local ip
    address.sin_port = htons(PORT); // use port 21333

    bind(server_fd, (struct sockaddr *)&address, sizeof(address)); // bind socket to port
    listen(server_fd, 3); // allow up to 3 pending connections
    printf("S2 ready on port %d...\n", PORT); // log server is ready

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen); // wait for client
        printf("Connection received in S2.\n"); // got a new client

        // Peek to detect command type
        char peekbuf[BUFSIZE] = {0}; // temp space to peek into incoming message
        int peek_n = recv(client_fd, peekbuf, BUFSIZE, MSG_PEEK); // look at what client sent without removing it
        if (peek_n <= 0) {
            perror("Initial peek failed"); // if client disconnected or sent nothing
            close(client_fd); // close connection
            continue;
        }

            // ✅ dispfnames
            if (strncmp(peekbuf, "dispfnames ", 11) == 0) { // check if client wants list of files
                memset(buffer, 0, BUFSIZE); // clear buffer
                int n = read(client_fd, buffer, BUFSIZE); // read full command
                if (n <= 0) {
                    perror("Failed to read dispfnames command"); // read failed
                    close(client_fd); // close conn
                    continue;
                }
                buffer[n] = '\0'; // null terminate

                char path[512];
                sscanf(buffer, "dispfnames %s", path); // extract path from cmd
                printf("📂 dispfnames request for: %s\n", path); // log it

                char listcmd[1100];
                snprintf(listcmd, sizeof(listcmd), "ls -p \"%s\" 2>/dev/null | grep '\\.pdf$' | sort", path); // cmd to get only .pdf files sorted

                FILE *fp = popen(listcmd, "r"); // run shell cmd and read from it
                if (!fp) {
                    write(client_fd, "Failed to list files\n", 22); // send error to client
                    close(client_fd);
                    continue;
                }

                char filelist[BUFSIZE] = {0}; // will hold all filenames
                fread(filelist, 1, sizeof(filelist), fp); // get file list
                pclose(fp); // done reading

                if (strlen(filelist) == 0)
                    write(client_fd, "", 0); // nothing found, send empty string
                else
                    write(client_fd, filelist, strlen(filelist)); // send back list

                close(client_fd); // done with client
                continue;
            }

            // ✅ removef
            if (strncmp(peekbuf, "removef ", 8) == 0) { // check if it's remove request
                memset(buffer, 0, BUFSIZE); // clear buffer
                int n = read(client_fd, buffer, BUFSIZE); // read full command
                if (n <= 0) {
                    perror("Failed to read removef command"); // read failed
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0'; // null terminate
                char *filepath = buffer + 8; // skip "removef " part
                printf("🗑️ File operation request: %s\n", filepath); // log filepath

                if (remove(filepath) == 0) { // try deleting file
                    printf("✅ File deleted: %s\n", filepath); // log success
                    write(client_fd, "File deleted from S1\n", 22); // confirm to client
                } else {
                    perror("Delete failed"); // show error in terminal
                    write(client_fd, "File not found in S1\n", 22); // notify client
                }
                close(client_fd); // close connection
                continue;
            }

	// ✅ downltar
        if (strncmp(peekbuf, "downltar ", 9) == 0) { // check if tar download requested
            memset(buffer, 0, BUFSIZE); // clear buffer
            read(client_fd, buffer, BUFSIZE); // read full command (but we're ignoring its args here)

            char tarname[32], ext[10], path[256];

            strcpy(tarname, "pdf.tar"); // name of tar file to be created
            strcpy(ext, "*.pdf"); // what type to bundle
            strcpy(path, "/home/vyas46/S2"); // target path to search

            // build tar creation cmd using find
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "cd %s && tar -cf %s $(find . -name \"%s\")", path, tarname, ext);
            system(cmd); // run the command

            // open the tar file
            char tarpath[512];
            snprintf(tarpath, sizeof(tarpath), "%s/%s", path, tarname); // full path of tar file
            FILE *fp = fopen(tarpath, "rb");
            if (!fp) {
                write(client_fd, "0", 1); // send failure
                close(client_fd);
                continue;
            }

            // get size and send to client
            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp); // get file size
            rewind(fp);
            char sizebuf[20];
            sprintf(sizebuf, "%ld", fsize); // turn to string
            write(client_fd, sizebuf, strlen(sizebuf)); // send size
            usleep(100000); // short wait

            // send content of tar file
            size_t n;
            while ((n = fread(buffer, 1, BUFSIZE, fp)) > 0)
                write(client_fd, buffer, n);
            fclose(fp); // done reading file
            close(client_fd); // done with client
            continue;
        }

            // ✅ downlf
            else if (strncmp(peekbuf, "/home/", 6) == 0) { // check if path sent means direct download
                memset(buffer, 0, BUFSIZE);
                int n = read(client_fd, buffer, BUFSIZE); // get full file path
                if (n <= 0) {
                    perror("Failed to read downlf path");
                    close(client_fd);
                    continue;
                }
                buffer[n] = '\0'; // null terminate
                printf("📥 downlf request: %s\n", buffer); // print path we got

                FILE *fp = fopen(buffer, "rb"); // try opening file
                if (!fp) {
                    write(client_fd, "0", 1); // send fail
                    close(client_fd);
                    continue;
                }

                fseek(fp, 0, SEEK_END); // go to end
                long fsize = ftell(fp); // get size
                rewind(fp); // back to top
                char sizebuf[20];
                sprintf(sizebuf, "%ld", fsize); // convert to string
                write(client_fd, sizebuf, strlen(sizebuf)); // send to client
                usleep(100000); // wait a bit

                long sent = 0;
                while (sent < fsize) {
                    int count = fread(buffer, 1, BUFSIZE, fp); // read chunk
                    write(client_fd, buffer, count); // send chunk
                    sent += count; // update total sent
                }
                fclose(fp); // done
                printf("✅ Sent file (%ld bytes)\n", fsize); // log success
                close(client_fd);
                continue;
            }

            // ✅ uploadf
            memset(buffer, 0, BUFSIZE); // reset buffer
            int n = read(client_fd, buffer, BUFSIZE); // read filename from client
            if (n <= 0) {
                perror("Filename read failed"); // if nothing received
                close(client_fd); // kill connection
                continue;
            }
            buffer[n] = '\0'; // terminate the string
            char filename[256];
            strcpy(filename, buffer); // copy file name
            printf("📤 Filename: %s\n", filename); // print name

            memset(buffer, 0, BUFSIZE); // clear again to read next
            n = read(client_fd, buffer, BUFSIZE); // read destination path
            if (n <= 0) {
                perror("Path read failed");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // terminate string
            char destpath[512];
            strcpy(destpath, buffer); // copy path
            printf("📁 Destination path: %s\n", destpath);

            memset(buffer, 0, BUFSIZE); // now read file size
            n = read(client_fd, buffer, BUFSIZE); // get size
            if (n <= 0) {
                perror("File size read failed");
                close(client_fd);
                continue;
            }
            buffer[n] = '\0'; // finish string
            long fsize = atol(buffer); // convert to number
            printf("📦 File size: %ld bytes\n", fsize); // log file size

            // make dir if not already there
            char mkdir_cmd[1024];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", destpath); // prep mkdir cmd
            system(mkdir_cmd); // create folder

            // build full file path where it'll be stored
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", destpath, filename); // create full path
            FILE *fp = fopen(filepath, "wb"); // open file for writing
            if (!fp) {
                perror("fopen failed"); // something went wrong
                write(client_fd, "File write error\n", 18); // notify client
                close(client_fd);
                continue;
            }

            // read file data from client and write to disk
            long received = 0;
            while (received < fsize) {
                n = read(client_fd, buffer, BUFSIZE); // read some bytes
                if (n <= 0) break;
                fwrite(buffer, 1, n, fp); // save to file
                received += n; // update count
            }
            fclose(fp); // close file when done

            printf("✅ Stored file: %s\n", filepath); // show saved file path
            write(client_fd, "File saved in S1\n", 18); // tell client upload worked
            close(client_fd); // close socket
        }
    }
