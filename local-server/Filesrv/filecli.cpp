//
// A simple file client application.
// It connects to a file server
// and sends commands to a server.
//
// Usage:
//          client [IP_address_of_server [port_of_server]]
//      where IP_address_of_server is either IP number of server
//      or a symbolic Internet name, default is "localhost";
//      port_of_server is a port number, default is 1234.
//
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <dirent.h>
#include <setjmp.h>
#include <limits.h>
#include "filesrv.h"

static void usage();
static void help();
static void initializeFrame(ProtocolFrame* frame);

class ProtocolException {
public:
    const char *reason;
    ProtocolException():
        reason("")
    {}

    ProtocolException(const char *cause):
        reason(cause)
    {}
};

static int receiveFrame(int s, ProtocolFrame* frame);
static void sendFrame(int s, const ProtocolFrame* frame) 
    throw (ProtocolException);

// Protocol commands
static void pwd(int s) throw (ProtocolException);
static void cd(int s, const char* path) throw (ProtocolException);
static void ls(int s) throw (ProtocolException);
static void put(int s, const char* path) throw (ProtocolException);
static void get(int s, const char* path) throw (ProtocolException);
static void logout(int s) throw (ProtocolException);

static const int BLOCK_SIZE = 1024;

static jmp_buf readCommandPoint;

int main(int argc, char *argv[]) {
    if (argc > 1 && *(argv[1]) == '-') {
        usage(); exit(1);
    }

    // Create a socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("Cannot create a socket"); exit(1);
    }

    // Fill in the address of server
    struct sockaddr_in peeraddr;
    memset(&peeraddr, 0, sizeof(peeraddr));
    const char* peerHost = "localhost";
    if (argc > 1)
        peerHost = argv[1];

    // Resolve the server address (convert from symbolic name to IP number)
    struct hostent *host = gethostbyname(peerHost);
    if (host == NULL) {
        perror("Cannot define host address"); exit(1);
    }
    peeraddr.sin_family = AF_INET;
    short peerPort = 1234;
    if (argc >= 3)
        peerPort = (short) atoi(argv[2]);
    peeraddr.sin_port = htons(peerPort);

    // Print a resolved address of server (the first IP of the host)
    printf(
        "peer addr = %d.%d.%d.%d, port %d\n",
        host->h_addr_list[0][0] & 0xff,
        host->h_addr_list[0][1] & 0xff,
        host->h_addr_list[0][2] & 0xff,
        host->h_addr_list[0][3] & 0xff,
        (int) peerPort
    );

    // Write resolved IP address of a server to the address structure
    memmove(&(peeraddr.sin_addr.s_addr), host->h_addr_list[0], 4);

    // Connect to a remote server
    int res = connect(s, (struct sockaddr*) &peeraddr, sizeof(peeraddr));
    if (res < 0) {
        perror("Cannot connect"); exit(1);
    }
    printf("Connected.\n");

    ProtocolFrame frame;

    try {

        while (
            receiveFrame(s, &frame) >= 0 && 
            frame.command != FSR_LOGOUT
        ) {
            char line[256];

            // Save an entry point for a long jump
            setjmp(readCommandPoint);

            LReadCommand: ;
            printf("Command>");
            if (fgets(line, 254, stdin) == NULL)
                break;

            // Parse a command
            line[254] = 0;
            int len = strlen(line);
            // Remove "\r\n" at the end of line
            if (len > 0 && line[len-1] == '\n') {
                line[len-1] = 0; --len;
            }
            if (len > 0 && line[len-1] == '\r') {
                line[len-1] = 0; --len;
            }
            if (len == 0)   // Empty command
                goto LReadCommand;

            int i = 0;
            // Skip a space in beginning of line
            while (i < len && isspace(line[i])) ++i;
            int commandBeg = i;
            while (i < len && isalpha(line[i])) ++i;
            int commandEnd = i;
            int commandLen = commandEnd - commandBeg;

            if (
                strncmp("pwd", line+commandBeg, commandLen) == 0
            ) {
                pwd(s);
            } else if (
                strncmp("cd", line+commandBeg, commandLen) == 0
            ) {
                while (i < len && isspace(line[i])) ++i; // Skip a space
                if (i >= len) {
                    printf("Incorrect command.\n");
                    goto LReadCommand;
                }

                cd(s, line + i);
            } else if (
                strncmp("ls", line+commandBeg, commandLen) == 0
            ) {
                ls(s);
            } else if (
                strncmp("put", line+commandBeg, commandLen) == 0
            ) {
                while (i < len && isspace(line[i])) ++i; // Skip a space
                if (i >= len) {
                    printf("Incorrect command.\n");
                    goto LReadCommand;
                }

                put(s, line + i);
            } else if (
                strncmp("get", line+commandBeg, commandLen) == 0
            ) {
                while (i < len && isspace(line[i])) ++i; // Skip a space
                if (i >= len) {
                    printf("Incorrect command.\n");
                    goto LReadCommand;
                }

                get(s, line + i);
            } else if (
                strncmp("logout", line+commandBeg, commandLen) == 0 ||
                line[commandBeg] == 'q' || line[commandBeg] == 'Q'
            ) {
                logout(s);
                break;
            } else {
                help();
                goto LReadCommand;
            }
        }

    } catch (ProtocolException& e) {
        fprintf(stderr, "%s\n", e.reason);
        close(s);
        return 1;
    }

    close(s);
    return 0;
}

static void usage() {
    printf(
        "A simple file client application.\n"
        "Usage:\n"
        "         client [IP_address_of_server [port_of_server]]\n"
        "     where IP_address_of_server is either IP number of server\n"
        "     or a symbolic Internet name, default is \"localhost\";\n"
        "     port_of_server is a port number, default is 1234.\n"
    );
}

static void help() {
    printf(
        "The following commands are possible:\n"
        "    pwd\t\t\tPrint Working Directory\n"
        "    cd\t\t\tChange a Directory\n"
        "    ls\t\t\tList a directory contents\n"
        "    put file\t\tPut a file to a server\n"
        "    get file\t\tGet a file from a server\n"
        "    logout, quit\tTerminate the program\n"
    );
}

static void initializeFrame(ProtocolFrame* frame) {
    memset(frame, 0, sizeof(ProtocolFrame));
}

static void sendFrame(int s, const ProtocolFrame* frame) 
    throw (ProtocolException) {
    ssize_t res = write(s, frame, FSR_HEADERLEN + frame->length);
    if (res < 0)
        throw ProtocolException("Write error on socket");
}

static int receiveFrame(int s, ProtocolFrame* frame) {
    // Read frame header
    //... ssize_t res = read(s, frame, FSR_HEADERLEN);
    ssize_t res = recv(s, frame, FSR_HEADERLEN, MSG_WAITALL);
    if (res < FSR_HEADERLEN)
        return (-1);
    int len = frame->length;
    if (len > FSR_MAXDATALEN)
        len = FSR_MAXDATALEN;
    char *pos = frame->data;
    while (len > 0) {
        int bytesToRead = len;
        if (bytesToRead > BLOCK_SIZE)
            bytesToRead = BLOCK_SIZE;
        res = read(s, pos, bytesToRead);
        if (res <= 0)
            return (-1);
        len -= res;
        pos += res;
    }
    return (FSR_HEADERLEN + frame->length);
}

static void pwd(int s) throw (ProtocolException) {
    ProtocolFrame frame;
    initializeFrame(&frame);
    frame.command = FSR_PWD;
    frame.length = 0;
    sendFrame(s, &frame);
    if (receiveFrame(s, &frame) < 0) {
        printf("Disconnected.\n");
        exit(1);
    }
    if (frame.command == FSR_DATAEND) {
        char path[PATH_MAX + 1];
        int len = frame.length;
        if (len > PATH_MAX)
            len = PATH_MAX;
        memmove(path, frame.data, len);
        path[len] = 0;
        printf("%s\n", path);
    } else if (frame.command == FSR_ERROR) {
        printf("Error: %s\n", frame.data);
    } else {
        printf("Incorrect protocol.\n");
        exit(1);
    }
}

static void cd(int s, const char* path) throw (ProtocolException) {
    ProtocolFrame frame;
    initializeFrame(&frame);
    frame.command = FSR_CD;
    int len = strlen(path);
    if (len >= FSR_MAXDATALEN)
        len = FSR_MAXDATALEN - 1;
    frame.length = len + 1;
    memmove(frame.data, path, len);
    frame.data[len] = 0;
    sendFrame(s, &frame);

    if (receiveFrame(s, &frame) < 0) {
        printf("Disconnected.\n");
        exit(1);
    }
    if (frame.command == FSR_SUCCESS) {
        printf("OK\n");
    } else if (frame.command == FSR_ERROR) {
        printf("Error: %s\n", frame.data);
    } else {
        printf("Incorrect protocol.\n");
        exit(1);
    }
}

static void ls(int /* s */) throw (ProtocolException) {
    // To do...
    printf("Not implemented.\n");
    longjmp(readCommandPoint, 0);
}

static void put(int /* s */, const char* /* path */) throw (ProtocolException) {
    // To do...
    printf("Not implemented.\n");
    longjmp(readCommandPoint, 0);
}

static void get(int /* s */, const char* /* path */) throw (ProtocolException) {
    // To do...
    printf("Not implemented.\n");
    longjmp(readCommandPoint, 0);
}

static void logout(int s) throw (ProtocolException) {
    ProtocolFrame frame;
    initializeFrame(&frame);
    frame.command = FSR_LOGOUT;
    frame.length = 0;
    sendFrame(s, &frame);
}
