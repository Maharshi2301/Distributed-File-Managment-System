// w25clients.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 19456
#define BUFSIZE 1024

int main()
{
    struct sockaddr_in serv_addr;
    char buffer[BUFSIZE], input[BUFSIZE];

    while (1)
    {
        printf("w25clients$ ");          // show prompt to user
        fgets(input, BUFSIZE, stdin);    // take user input
        input[strcspn(input, "\n")] = 0; // remove newline char

        if (strcmp(input, "exit") == 0)
            break; // if user types exit, stop loop

        // Open a new socket for each command
        int sock = socket(AF_INET, SOCK_STREAM, 0); // create socket
        if (sock < 0)
        {
            printf("Socket creation failed"); // if socket fails
            continue;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);                     // use defined port
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // convert IP to binary

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("Connection to server failed"); // server not reachable
            close(sock);
            continue;
        }

        // Handle uploadf command separately
        if (strncmp(input, "uploadf ", 8) == 0)
        { // check if it's upload command
            char filename[256], destpath[512];
            if (sscanf(input, "uploadf %s %s", filename, destpath) != 2)
            {
                printf("Invalid syntax. Use: uploadf <filename> <dest_path>\n"); // wrong command format
                close(sock);
                continue;
            }

            FILE *fp = fopen(filename, "rb"); // try to open the file
            if (!fp)
            {
                printf("File open failed"); // file not found
                close(sock);
                continue;
            }

            // Send command
            send(sock, input, strlen(input), 0); // send the uploadf command
            usleep(100000);                      // wait a bit

            // Send file size
            fseek(fp, 0, SEEK_END); // go to end of file
            long fsize = ftell(fp); // get file size
            rewind(fp);             // go back to beginning
            char fsize_str[20];
            sprintf(fsize_str, "%ld", fsize);            // turn number into string
            send(sock, fsize_str, sizeof(fsize_str), 0); // send file size
            usleep(100000);                              // pause a sec

            // Send file content
            size_t n;
            while ((n = fread(buffer, 1, BUFSIZE, fp)) > 0)
            {                             // read in chunks
                send(sock, buffer, n, 0); // send each chunk
            }
            fclose(fp); // done reading file

            // Read server response
            memset(buffer, 0, BUFSIZE);              // clear for response
            int bytes = read(sock, buffer, BUFSIZE); // wait for reply
            if (bytes <= 0)
            {
                printf("Read failed or server disconnected"); // something went wrong
                close(sock);
                continue;
            }
            buffer[bytes] = '\0';       // null-terminate
            printf("S1: %s\n", buffer); // print server reply

            close(sock); // done with this request
            continue;
        } // end of uploadf

        // downlf logic
        if (strncmp(input, "downlf ", 7) == 0)
        { // check if user wants to download a file
            // Create a new socket
            sock = socket(AF_INET, SOCK_STREAM, 0); // new socket for this cmd
            if (sock < 0)
            {
                printf("Socket creation failed");
                continue;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // connect to localhost

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                printf("Connection to server failed"); // can't reach server
                close(sock);
                continue;
            }

            // Send the downlf command
            send(sock, input, strlen(input), 0); // send full command like "downlf ~S1/abc.txt"
            usleep(100000);                      // short pause

            // Receive file size
            char sizebuf[20];
            int n = read(sock, sizebuf, sizeof(sizebuf)); // get size of file
            if (n <= 0)
            {
                printf("Failed to receive file size");
                close(sock);
                continue;
            }

            sizebuf[n] = '\0';          // null terminate
            long fsize = atol(sizebuf); // convert size to long
            if (fsize <= 0)
            {
                printf("File not found or error fetching file.\n"); // file doesn't exist
                close(sock);
                continue;
            }

            // Extract filename from input path
            char *filepath = input + 7;              // skip "downlf "
            char *filename = strrchr(filepath, '/'); // get filename from full path
            if (!filename)
                filename = filepath; // no '/' in path
            else
                filename++; // move past '/'

            FILE *fp = fopen(filename, "wb"); // open file locally for writing
            if (!fp)
            {
                printf("Failed to create file locally");
                close(sock);
                continue;
            }

            // read and write the file in chunks
            long received = 0;
            while (received < fsize)
            {
                n = read(sock, buffer, BUFSIZE); // get data
                if (n <= 0)
                    break;
                fwrite(buffer, 1, n, fp); // save it
                received += n;
            }
            fclose(fp); // done

            printf("✅ File '%s' downloaded successfully (%ld bytes)\n", filename, fsize); // success msg
            close(sock);
            continue;
        } // end of downlf

        // removef logic
        else if (strncmp(input, "removef ", 8) == 0)
        { // check for delete request
            // Create new socket
            sock = socket(AF_INET, SOCK_STREAM, 0); // make socket
            if (sock < 0)
            {
                printf("Socket creation failed");
                continue;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT);
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // connect to localhost

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                printf("Connection to server failed"); // server unreachable
                close(sock);
                continue;
            }

            // Send the command
            send(sock, input, strlen(input), 0); // forward removef command
            usleep(100000);                      // give it a moment

            // Read response
            memset(buffer, 0, BUFSIZE);
            int bytes = read(sock, buffer, BUFSIZE); // wait for reply
            if (bytes > 0)
            {
                buffer[bytes] = '\0';

                // Check if server response indicates failure
                if (strstr(buffer, "not found") || strstr(buffer, "Not found"))
                {
                    printf("File not found or error fetching file.\n"); // show clean error
                }
                else
                {
                    printf("S1: %s\n", buffer); // show actual message
                }
            }
            else
            {
                printf("Failed to receive response"); // nothing came back
            }

            close(sock); // done
            continue;
        } // end of remove file

        else if (strncmp(input, "dispfnames ", 11) == 0)
        { // check if it's a file list request
            // Create socket
            sock = socket(AF_INET, SOCK_STREAM, 0); // new socket
            if (sock < 0)
            {
                printf("Socket creation failed");
                continue;
            }

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(PORT);                     // set port
            inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr); // localhost

            if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
            {
                printf("Connection to server failed");
                close(sock);
                continue;
            }

            // Send command to S1
            send(sock, input, strlen(input), 0); // send dispfnames cmd
            usleep(100000);                      // wait a bit

            // Read response
            memset(buffer, 0, BUFSIZE);              // clear buffer
            int bytes = read(sock, buffer, BUFSIZE); // read file list
            if (bytes > 0)
            {
                buffer[bytes] = '\0'; // null terminate

                // check if it's empty or failed
                if (strstr(buffer, "No files found") || strstr(buffer, "Failed to list files"))
                    printf("❌ %s\n", buffer); // show problem
                else
                {
                    printf("📂 Files in directory (grouped by type):\n%s\n", buffer); // print files
                }
            }
            else
            {
                printf("❌ Failed to receive file list.\n"); // read failed
            }

            close(sock); // done with socket
            continue;
        } // end of dispfnames

        else if (strncmp(input, "downltar ", 9) == 0)
        { // handle tar file download
            char filetype[10];
            sscanf(input, "downltar %s", filetype); // get file extension

            char tarname[32];
            if (strcmp(filetype, ".c") == 0)
                strcpy(tarname, "cfiles.tar");
            else if (strcmp(filetype, ".pdf") == 0)
                strcpy(tarname, "pdf.tar");
            else if (strcmp(filetype, ".txt") == 0)
                strcpy(tarname, "text.tar");
            else
            {
                printf("Unsupported filetype for downltar\n"); // invalid extension
                close(sock);
                continue;
            }

            send(sock, input, strlen(input), 0); // send command to server
            usleep(100000);                      // short wait

            char sizebuf[20];
            int n = read(sock, sizebuf, sizeof(sizebuf)); // get size of tar file
            if (n <= 0)
            {
                printf("Failed to get size\n");
                close(sock);
                continue;
            }
            sizebuf[n] = '\0';
            long fsize = atol(sizebuf); // convert to long
            if (fsize <= 0)
            {
                printf("No such files to archive\n"); // nothing to send
                close(sock);
                continue;
            }

            FILE *fp = fopen(tarname, "wb"); // create local file to store tar
            if (!fp)
            {
                printf("Failed to create tar file locally");
                close(sock);
                continue;
            }

            long received = 0;
            while (received < fsize)
            { // receive tar in chunks
                n = read(sock, buffer, BUFSIZE);
                fwrite(buffer, 1, n, fp);
                received += n;
            }
            fclose(fp);                                                         // done
            printf("✅ Tarball '%s' downloaded (%ld bytes)\n", tarname, fsize); // confirm to user
            close(sock);                                                        // close conn
            continue;
        } // End of downltar function

        // For other commands (not handled here yet)
        send(sock, input, strlen(input), 0);     // just send the input to server
        memset(buffer, 0, BUFSIZE);              // clear buffer before reading reply
        int bytes = read(sock, buffer, BUFSIZE); // try to get server response
        if (bytes > 0)
        {
            buffer[bytes] = '\0';                // null-terminate it
            printf("S1 response: %s\n", buffer); // print what server replied
        }
        else
        {
            printf("Read failed or server disconnected"); // handle no reply
        }

        close(sock); // done with socket for this command
    }

    return 0; // end of main loop
}
