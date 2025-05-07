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

    // Get file extension
    char *ext = strrchr(filename, '.');
    if (!ext) ext = ""; // No extension

    // Receive file size
    char sizebuf[20] = {0};
    read(client_fd, sizebuf, sizeof(sizebuf));
    long fsize = atol(sizebuf);

    // Replace ~S1 with actual path
    char fullpath[1024];
    if (strncmp(destpath, "~S1", 3) == 0)
        snprintf(fullpath, sizeof(fullpath), "/home/vyas46/S1%s", destpath + 3);
    else
        snprintf(fullpath, sizeof(fullpath), "%s", destpath);

    // Create directories recursively
    char mkdir_cmd[1100];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p \"%s\"", fullpath);
    system(mkdir_cmd);

    // If it's a .c file — store locally
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
    } else {
        int target_port = 0;
char server_path_prefix[256];

if (strcmp(ext, ".pdf") == 0) {
    target_port = 9002;
    strcpy(server_path_prefix, "/home/vyas46/S2"); // adjust accordingly
} else if (strcmp(ext, ".txt") == 0) {
    target_port = 9003;
    strcpy(server_path_prefix, "/home/vyas46/S3");
} else if (strcmp(ext, ".zip") == 0) {
    target_port = 9004;
    strcpy(server_path_prefix, "/home/vyas46/S4");
} else {
    write(client_fd, "Unsupported file type", 22);
    return;
}

// connect to backend server
int sock = socket(AF_INET, SOCK_STREAM, 0);
struct sockaddr_in serv_addr;
serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(target_port);
inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

// forward: filename
send(sock, filename, strlen(filename), 0);
usleep(100000);

// forward: updated path (replace ~S1 with appropriate)
char actual_path[1024];
snprintf(actual_path, sizeof(actual_path), "%s%s", server_path_prefix, destpath + 3);
send(sock, actual_path, strlen(actual_path), 0);
usleep(100000);

// forward: file size
char fsize_str[20];
sprintf(fsize_str, "%ld", fsize);
send(sock, fsize_str, strlen(fsize_str), 0);
usleep(100000);

// forward: file content
long remaining = fsize;
char filebuf[BUFSIZE];
while (remaining > 0) {
    int n = read(client_fd, filebuf, BUFSIZE);
    send(sock, filebuf, n, 0);
    remaining -= n;
}

// cleanup
read(sock, filebuf, BUFSIZE);  // read server acknowledgment
write(client_fd, filebuf, strlen(filebuf)); // send result to client
close(sock);

        //write(client_fd, "Forwarding to other server (coming next!)", 42);
    }
}


        write(client_fd, "OK from S1", strlen("OK from S1"));
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
