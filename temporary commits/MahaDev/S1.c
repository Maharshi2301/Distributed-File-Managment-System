// S1.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


#define PORT 19456
#define BUFSIZE 1024

char *get_remote_list(const char *remote_path, int port);

void prcclient(int client_fd) {
    char buffer[BUFSIZE];
    while (1) {
        // clear the buffer every time
        memset(buffer, 0, BUFSIZE);
        
        // read cmd from client
        int bytes = read(client_fd, buffer, BUFSIZE); // read what client sent
        if (bytes <= 0) break; // if client disconnected

        // show what we got from client
        printf("Received from client: %s\n", buffer);
        fflush(stdout);

        // handle uploadf command
        if (strncmp(buffer, "uploadf ", 8) == 0) {
            // grab filename and path from msg
            char filename[256], destpath[512];
            sscanf(buffer, "uploadf %s %s", filename, destpath); // pull both strings from input

            // pull file ext like .txt or .pdf
            char *ext = strrchr(filename, '.'); // find last dot in filename
            if (!ext) ext = ""; // no ext fallback

            // get file size
            char sizebuf[20] = {0};
            read(client_fd, sizebuf, sizeof(sizebuf)); // get size as string
            long fsize = atol(sizebuf); // convert to long

            // figure out the full folder path where to save
            char fullpath[1024];
            if (strncmp(destpath, "~S1", 3) == 0)
                snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", destpath + 3); // cut off ~S1 and attach real path
            else
                snprintf(fullpath, sizeof(fullpath), "%s", destpath); // use as is

            // make that folder if it's not already there
            char mkdir_cmd[1100];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", fullpath); // prep mkdir command
            system(mkdir_cmd); // actually make the dir

            // if it's a .c file, keep it in s1
            if (strcmp(ext, ".c") == 0) {
                // build the file's full path
                char filepath[1100];
                snprintf(filepath, sizeof(filepath), "%s/%s", fullpath, filename); // full file location
                FILE *fp = fopen(filepath, "wb"); // try to open file for writing binary
                if (!fp) {
                    write(client_fd, "Failed to create file", 22); // tell client something went wrong
                    return;
                }

                // read file content in chunks from client and write to file
                long received = 0;
                char filebuf[BUFSIZE];
                while (received < fsize) {
                    int n = read(client_fd, filebuf, BUFSIZE); // get part of file
                    fwrite(filebuf, 1, n, fp); // write to file
                    received += n;
                }
                fclose(fp); // close file

                // tell client we're done
                write(client_fd, "File stored in S1", 18); // send success msg
                continue;
            } else {
                // for other files, like .pdf .txt .zip
                int target_port = 0;
                char server_path_prefix[256];

                // pick port and server path based on file type
                if (strcmp(ext, ".pdf") == 0) {
                    target_port = 21333; // pdf files → s2
                    strcpy(server_path_prefix, "/home/vyas46/S2");
                } else if (strcmp(ext, ".txt") == 0) {
                    target_port = 21777; // txt → s3
                    strcpy(server_path_prefix, "/home/vyas46/S3");
                } else if (strcmp(ext, ".zip") == 0) {
                    target_port = 22123; // zip → s4
                    strcpy(server_path_prefix, "/home/vyas46/S4");
                } else {
                    // if type isn't handled
                    write(client_fd, "Unsupported file type", 22); // tell client
                    continue;
                }

                // setup socket to backend server
                int sock = socket(AF_INET, SOCK_STREAM, 0); // create socket
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(target_port); // set port
                inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // connect to localhost
                connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); // connect to backend

                // send file name
                send(sock, filename, strlen(filename), 0); // send just the name
                usleep(100000); // pause a bit

                // send adjusted full path
                char actual_path[1024];
                snprintf(actual_path, sizeof(actual_path), "%s%s", server_path_prefix, destpath + 3); // convert ~S1 to backend path
                send(sock, actual_path, strlen(actual_path), 0); // send it
                usleep(100000);

                // send file size as a string
                char fsize_str[20];
                sprintf(fsize_str, "%ld", fsize); // convert to str
                send(sock, fsize_str, strlen(fsize_str), 0);
                usleep(100000);

                // now stream file content from client and pass to backend
                long remaining = fsize;
                char filebuf[BUFSIZE];
                while (remaining > 0) {
                    int n = read(client_fd, filebuf, BUFSIZE); // get next piece
                    send(sock, filebuf, n, 0); // send to backend
                    remaining -= n; // update how much left
                }

                // clean buffer before reading backend response
                memset(filebuf, 0, BUFSIZE); // reset

                // wait for backend reply (like "file saved in S2")
                int resp_bytes = read(sock, filebuf, BUFSIZE); // get response
                if (resp_bytes > 0) {
                    filebuf[resp_bytes] = '\0'; // null terminate
                    write(client_fd, filebuf, strlen(filebuf)); // send back to client
                }

                // done with backend socket
                close(sock);
                continue;
            }
        }// end of uploadf


        else if (strncmp(buffer, "downlf ", 7) == 0) { // check if client asked to download a file
            char filepath[512];
            sscanf(buffer, "downlf %s", filepath); // grab filepath from client msg

            char *ext = strrchr(filepath, '.'); // get file extension from end of filename
            if (!ext) {
                write(client_fd, "0", 1); // invalid extension
                continue;
            }

            // make full path either from ~S1 or absolute
            char fullpath[1024];
            if (strncmp(filepath, "~S1", 3) == 0)
                snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", filepath + 3); // add to s1 path
            else
                snprintf(fullpath, sizeof(fullpath), "%s", filepath); // use path as is

            // if it's .c, read directly from s1 storage
            if (strcmp(ext, ".c") == 0) {
                FILE *fp = fopen(fullpath, "rb"); // try opening file
                if (!fp) {
                    write(client_fd, "0", 1); // tell client we couldn't open it
                    continue;
                }

                fseek(fp, 0, SEEK_END); // go to end of file
                long fsize = ftell(fp); // get file size
                rewind(fp); // back to start
                char fsize_str[20];
                sprintf(fsize_str, "%ld", fsize); // convert size to string
                write(client_fd, fsize_str, strlen(fsize_str)); // send size to client
                usleep(100000); // wait a bit before sending file

                char buf[BUFSIZE];
                size_t n;
                while ((n = fread(buf, 1, BUFSIZE, fp)) > 0) { // read and send chunks
                    write(client_fd, buf, n); // send data to client
                }
                fclose(fp); // done with file
                continue;
            } else {
                // for pdf, txt, zip send to backend
                int target_port = 0;
                if (strcmp(ext, ".pdf") == 0) target_port = 21333; // s2
                else if (strcmp(ext, ".txt") == 0) target_port = 21777; // s3
                else if (strcmp(ext, ".zip") == 0) target_port = 22123; // s4
                else {
                    write(client_fd, "0", 1); // unsupported file type
                    continue;
                }

                // setup socket to backend
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(target_port); // set target port
                inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // connect to localhost backend

                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    perror("Failed to connect to backend"); // if connect fails
                    write(client_fd, "0", 1); // tell client it failed
                    close(sock);
                    continue;
                }

                // build backend file path
                char remote_path[1024];
                if (strcmp(ext, ".pdf") == 0)
                    snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S2/%s", filepath + 4); // replace ~S1
                else if (strcmp(ext, ".txt") == 0)
                    snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S3/%s", filepath + 4);
                else if (strcmp(ext, ".zip") == 0)
                    snprintf(remote_path, sizeof(remote_path), "/home/vyas46/S4/%s", filepath + 4);

                write(sock, remote_path, strlen(remote_path)); // tell backend what to send
                usleep(100000); // short delay

                char sizebuf[20];
                int n = read(sock, sizebuf, sizeof(sizebuf)); // get file size from backend
                sizebuf[n] = '\0';
                long fsize = atol(sizebuf); // parse file size
                if (fsize <= 0) {
                    write(client_fd, "0", 1); // if size invalid, tell client
                    close(sock);
                    continue;
                }

                write(client_fd, sizebuf, strlen(sizebuf)); // send size to client
                usleep(100000);

                char buf[BUFSIZE];
                long received = 0;
                while (received < fsize) { // loop till we read whole file
                    int bytes = read(sock, buf, BUFSIZE);
                    if (bytes <= 0) break;
                    write(client_fd, buf, bytes); // send each chunk to client
                    received += bytes;
                }
                close(sock); // all done
                continue;
            }
        }// end of downlf


        else if (strncmp(buffer, "removef ", 8) == 0) { // check if client wants to delete file
            char filepath[512];
            sscanf(buffer, "removef %s", filepath); // extract file path from command

            char *ext = strrchr(filepath, '.'); // get file extension
            if (!ext) {
                write(client_fd, "Unsupported or missing file extension\n", 39); // no valid ext found
                continue;
            }

            // build full local file path
            char fullpath[1024];
            if (strncmp(filepath, "~S1", 3) == 0)
                snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", filepath + 3); // convert to full s1 path
            else
                snprintf(fullpath, sizeof(fullpath), "%s", filepath); // use path directly

            // if it's a .c file, delete locally
            if (strcmp(ext, ".c") == 0) {
                if (remove(fullpath) == 0) // try removing file
                    write(client_fd, "File deleted from S1\n", 22); // tell client success
                else
                    write(client_fd, "File not found in S1\n", 22); // tell client fail
                continue;
            }

            // Forward to S2/S3/S4 based on file type
            int target_port = 0;
            if (strcmp(ext, ".pdf") == 0) target_port = 21333; // s2
            else if (strcmp(ext, ".txt") == 0) target_port = 21777; // s3
            else if (strcmp(ext, ".zip") == 0) target_port = 22123; // s4
            else {
                write(client_fd, "Unsupported file type\n", 23); // unhandled ext
                continue;
            }

            // open socket to talk to backend server
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(target_port); // set port
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // local server

            // try connecting to backend
            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                perror("Connection to backend server failed"); // show backend connect fail
                write(client_fd, "Failed to connect to backend\n", 30);
                close(sock);
                continue;
            }

            // Adjust path before sending to backend
            char remote_path[1024];
            if (strcmp(ext, ".pdf") == 0)
                snprintf(remote_path, sizeof(remote_path), "removef /home/vyas46/S2/%s", filepath + 4); // s2 full path
            else if (strcmp(ext, ".txt") == 0)
                snprintf(remote_path, sizeof(remote_path), "removef /home/vyas46/S3/%s", filepath + 4); // s3 path
            else if (strcmp(ext, ".zip") == 0)
                snprintf(remote_path, sizeof(remote_path), "removef /home/vyas46/S4/%s", filepath + 4); // s4 path

            write(sock, remote_path, strlen(remote_path)); // send remove command to backend
            usleep(100000); // lil wait to sync

            char reply[BUFSIZE];
            int n = read(sock, reply, BUFSIZE); // get backend response
            if (n > 0) {
                reply[n] = '\0'; // null-terminate
                write(client_fd, reply, strlen(reply)); // send reply to client
            } else {
                write(client_fd, "You can not remove .zip file\n", 26); // if backend went silent
            }
            close(sock); // done with backend
            continue;
        }//end of remove file


        else if (strncmp(buffer, "dispfnames ", 11) == 0) { // check if user asked to display file names
            char user_path[512];
            sscanf(buffer, "dispfnames %s", user_path); // grab path from command

            // Extract relative path from ~S1
            char *subpath = user_path + 3; // skip ~S1 to get actual folder
            char full_local[512], full_s2[512], full_s3[512], full_s4[512];
            snprintf(full_local, sizeof(full_local), "/home/vyas46/S1%s", subpath); // s1 local folder
            snprintf(full_s2, sizeof(full_s2), "/home/vyas46/S2%s", subpath); // s2 full path
            snprintf(full_s3, sizeof(full_s3), "/home/vyas46/S3%s", subpath); // s3 path
            snprintf(full_s4, sizeof(full_s4), "/home/vyas46/S4%s", subpath); // s4 path

            // Step 1: get .c files locally
            char cmd_c[600];
            snprintf(cmd_c, sizeof(cmd_c), "ls -p \"%s\" 2>/dev/null | grep '\\.c$' | sort", full_local); // cmd to list .c files
            FILE *fp_c = popen(cmd_c, "r"); // open a pipe to read output
            char c_files[BUFSIZE] = {0};
            if (fp_c) {
                fread(c_files, 1, sizeof(c_files), fp_c); // read the list
                pclose(fp_c); // done with it
            }

            // Step 3: define list for all the files
            char combined[BUFSIZE * 2] = {0}; // this will hold final list
            strcat(combined, c_files); // add .c files first

            // Step 4: get other types from S2, S3 and S4 and add it into the list
            char *pdf_files = get_remote_list(full_s2, 21333); // ask s2 for .pdf files
            strcat(combined, pdf_files); // add to final list
            char *txt_files = get_remote_list(full_s3, 21777); // ask s3 for .txt files
            strcat(combined, txt_files); // add to final list
            char *zip_files = get_remote_list(full_s4, 22123); // ask s4 for .zip files
            strcat(combined, zip_files); // add to final list

            // Step 5: send to client
            if (strlen(combined) == 0)
                write(client_fd, "No files found.\n", 16); // nothing to show
            else
                write(client_fd, combined, strlen(combined)); // send file names

            continue;
        }// end of displfnames

        else if (strncmp(buffer, "downltar ", 9) == 0) { // check if command is downltar
            char filetype[10];
            sscanf(buffer, "downltar %s", filetype); // grab filetype like .c or .pdf

            char cmd[1024], tarname[32];
            int backend_port = 0;
            char *backend_path = NULL;

            // check which file type we want and where to handle it
            if (strcmp(filetype, ".c") == 0) {
                    // for .c files, create tar here in s1
                    strcpy(tarname, "cfiles.tar"); // name of tar file
                    snprintf(cmd, sizeof(cmd), "cd /home/vyas46/S1 && tar -cf %s $(find . -name \"*.c\")", tarname); // tar cmd for .c
                    system(cmd); // run the tar command
            } else if (strcmp(filetype, ".pdf") == 0) {
                    backend_port = 21333; // send to s2
                    strcpy(tarname, "pdf.tar");
            } else if (strcmp(filetype, ".txt") == 0) {
                    backend_port = 21777; // send to s3
                    strcpy(tarname, "text.tar");
            } else {
                    write(client_fd, "Unsupported type\n", 18); // not supported type
                    continue;
            }

            // If backend, forward request
            if (backend_port > 0) {
                    int sock = socket(AF_INET, SOCK_STREAM, 0); // create socket
                    struct sockaddr_in serv_addr;
                    serv_addr.sin_family = AF_INET;
                    serv_addr.sin_port = htons(backend_port); // use proper port
                    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // localhost

                    connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); // try to connect

                    send(sock, buffer, strlen(buffer), 0); // send "downltar .pdf" or .txt
                    usleep(100000); // wait a bit

                    // Receive size
                    char sizebuf[20];
                    int n = read(sock, sizebuf, sizeof(sizebuf)); // read size of file
                    sizebuf[n] = '\0';
                    long fsize = atol(sizebuf); // convert to number
                    write(client_fd, sizebuf, strlen(sizebuf)); // pass size to client
                    usleep(100000);

                    // Relay tar file
                    long received = 0;
                    while (received < fsize) { // read and forward until done
                            int n = read(sock, buffer, BUFSIZE);
                            write(client_fd, buffer, n);
                            received += n;
                    }
                    close(sock); // close socket
                    continue;
            }

            // Send file size for tar created on s1
            char path[512];
            snprintf(path, sizeof(path), "/home/vyas46/S1/%s", tarname); // make full path
            FILE *fp = fopen(path, "rb"); // open tar file
            if (!fp) {
                    write(client_fd, "0", 1); // send 0 if file missing
                    continue;
            }

            fseek(fp, 0, SEEK_END); // go to end
            long fsize = ftell(fp); // get size
            rewind(fp); // back to start
            char sizebuf[20];
            sprintf(sizebuf, "%ld", fsize); // put size in str
            write(client_fd, sizebuf, strlen(sizebuf)); // tell client how big file is
            usleep(100000);

            size_t n;
            while ((n = fread(buffer, 1, BUFSIZE, fp)) > 0) // read chunks and send to client
                    write(client_fd, buffer, n);
            fclose(fp); // done with file
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
	static char output[BUFSIZE]; // this holds what backend sends back
	memset(output, 0, BUFSIZE); // clean it before use

	int sock = socket(AF_INET, SOCK_STREAM, 0); // make a new socket
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port); // use given port
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // talk to localhost

	// try to connect to that backend
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(sock); // couldn't connect
		return output; // send back empty string
	}

	char cmd[600];
	snprintf(cmd, sizeof(cmd), "dispfnames %s", remote_path); // prepare command string
	send(sock, cmd, strlen(cmd), 0); // send it to backend
	usleep(100000); // short wait so backend gets ready

	read(sock, output, BUFSIZE); // read the list it sends
	close(sock); // all done with socket
	return output; // return what we got
}


int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // create main server socket
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // so we can reuse the address
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // listen on any local IP
    address.sin_port = htons(PORT); // use defined port

    bind(server_fd, (struct sockaddr *)&address, sizeof(address)); // bind server to addr
    listen(server_fd, 5); // start listening for up to 5 pending conns
    printf("S1 Server listening on port %d...\n", PORT);
	fflush(stdout);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen); // wait for new client
        if (fork() == 0) { // handle client in a new process
            close(server_fd); // child doesn't need server_fd
            prcclient(client_fd); // do client stuff
            exit(0); // done with child
        }

        close(client_fd); // parent doesn't need this client anymore
    }//end of while

}//end of main
