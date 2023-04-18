//
// A simple file server application.
// It listens to the port written in command line (default 1234),
// accepts a connection, and perform clients commands
//
// Usage:
//      server [port_to_listen]
// Default is the port 1234.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <assert.h>
#include <dirent.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include "filesrv.h"

static void sigHandler(int sigID);  // Handler of SIGCHLD signal
                                    // (used to prevent zombie children)
static void usage();
static void serveClient(int socket);
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

// Functions performing client commands:
static void receiveFile(int s,  ProtocolFrame* frame) throw (ProtocolException);
static void sendFile(int s, ProtocolFrame* frame) throw (ProtocolException);
static void pwd(int s, ProtocolFrame* frame) throw (ProtocolException);
static void cd(int s, ProtocolFrame* frame) throw (ProtocolException);
static void ls(int s, ProtocolFrame* frame) throw (ProtocolException);
static void protocolError(int s, const char *txt) throw (ProtocolException);
static void replySuccess(int s) throw (ProtocolException);

static const int BLOCK_SIZE = 1024;

int main(int argc, char *argv[]) {
    if (argc > 1 && *(argv[1]) == '-') {
        usage(); exit(1);
    }

    // Set signal handler for the "SIGCHLD" signal
    // (used to intercept the signal about child termination).
    if (signal(SIGCHLD, &sigHandler) == SIG_ERR) {
        perror("Cannot install a signal handler");
    }

    int listenPort = 1234;
    if (argc > 1)
        listenPort = atoi(argv[1]);

    // Create a socket
    int s0 = socket(AF_INET, SOCK_STREAM, 0);
    if (s0 < 0) {
        perror("Cannot create a socket"); exit(1);
    }

    // Fill in the address structure containing self address
    struct sockaddr_in myaddr;
    memset(&myaddr, 0, sizeof(struct sockaddr_in));
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(listenPort);        // Port to listen
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind a socket to the address
    int res = bind(s0, (struct sockaddr*) &myaddr, sizeof(myaddr));
    if (res < 0) {
        perror("Cannot bind a socket"); exit(1);
    }

    // Set the "LINGER" timeout to zero, to close the listen socket
    // immediately at program termination.
    struct linger linger_opt = { 1, 0 }; // Linger active, timeout 0
    setsockopt(s0, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

    // Now, listen for a connection
    res = listen(s0, 1);    // "1" is the maximal length of the queue
    if (res < 0) {
        perror("Cannot listen"); exit(1);
    }

    // Accept a connection (the "accept" command waits for a connection with
    // no timeout limit...)
    while (true) {
        struct sockaddr_in peeraddr;
        socklen_t peeraddr_len = sizeof(peeraddr);
        int s1 = accept(s0, (struct sockaddr*) &peeraddr, &peeraddr_len);
        if (s1 < 0) {
            perror("Cannot accept"); exit(1);
        }

        // A connection is accepted. The new socket "s1" is created
        // for data input/output. The peeraddr structure is filled in with
        // the address of connected entity, print it.
        printf(
            "Connection from IP %d.%d.%d.%d, port %d\n",
            (ntohl(peeraddr.sin_addr.s_addr) >> 24) & 0xff, // High byte of address
            (ntohl(peeraddr.sin_addr.s_addr) >> 16) & 0xff, // . . .
            (ntohl(peeraddr.sin_addr.s_addr) >> 8) & 0xff,  // . . .
            ntohl(peeraddr.sin_addr.s_addr) & 0xff,         // Low byte of addr
            ntohs(peeraddr.sin_port)
        );

        pid_t child_pid = fork();
        if (child_pid < 0) {
            perror("Could not fork");
            exit(1);
        }
        if (child_pid > 0) {
            // This is parent
            close(s1);
            continue;
        } else {
            close(s0);
            serveClient(s1);
            exit(0);
        }
    }

    close(s0);    // Close the listen socket
    return 0;
}

static void usage() {
    printf(
        "A test Internet file server.\n"
        "Usage:\n"
        "     server [port_to_listen]\n"
        "Default is the port 1234.\n"
    );
}

static void sigHandler(int /* sigID */) {
    // printf("The SIGCHLD signal (child exited).\n");
    // If we do not call "wait", a child becomes a zombie!
    int status = 0;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid == (-1))
        perror("Error waitpid");
}

static void serveClient(int s) {
    ProtocolFrame frame;
    try {
        initializeFrame(&frame);
        frame.command = FSR_READY;
        frame.length = 0;
        sendFrame(s, &frame);

        while (receiveFrame(s, &frame) >= 0 && frame.command != FSR_LOGOUT) {
            switch (frame.command) {
            case FSR_PUT:
                receiveFile(s, &frame);
                break;
            case FSR_GET:
                sendFile(s, &frame);
                break;
            case FSR_PWD:
                pwd(s, &frame);
                break;
            case FSR_CD:
                cd(s, &frame);
                break;
            case FSR_LS:
                ls(s, &frame);
                break;
            default:
                protocolError(s, "Illegal command");
                break;
            }

            initializeFrame(&frame);
            frame.command = FSR_READY;
            frame.length = 0;
            sendFrame(s, &frame);
        }
    } catch (ProtocolException& e) {
        fprintf(stderr, "%s\n", e.reason);
    }

    printf("Closing a connection.\n");

    close(s);   // Close a connection
}

static void initializeFrame(ProtocolFrame* frame) {
    memset(frame, 0, sizeof(ProtocolFrame));
}

static void receiveFile(int s,  ProtocolFrame* frame) 
    throw (ProtocolException) {
    printf("put %s\n", frame->data);

    char path[PATH_MAX+1];
    int len = frame->length;
    if (len > PATH_MAX)
        len = PATH_MAX;
    memmove(path, frame->data, len);
    path[len] = 0;
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        protocolError(s, strerror(errno));
        return;
    } else {
        initializeFrame(frame);
        frame->command = FSR_READY;
        frame->length = 0;
        sendFrame(s, frame);
    }

    int errnum = 0;
    while (true) {
        int res = receiveFrame(s, frame);
        if (res < 0)
            break;
        if (frame->command != FSR_DATA && frame->command != FSR_DATAEND)
            break;
        if (f != NULL) {
            if (fwrite(frame->data, 1, frame->length, f) <= 0) {
                errnum = errno;
                fclose(f); f = NULL;
            }
        }
        if (frame->command == FSR_DATAEND)
            break;
    }

    if (f != NULL) {
        fclose(f);
        replySuccess(s);
    } else {
        protocolError(s, strerror(errnum));
    }
}

static void sendFile(int s, ProtocolFrame* frame) 
    throw (ProtocolException) {
    printf("get %s\n", frame->data);

    char path[PATH_MAX+1];
    int len = frame->length;
    if (len > PATH_MAX)
        len = PATH_MAX;
    memmove(path, frame->data, len);
    path[len] = 0;
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        protocolError(s, strerror(errno));
        return;
    }
    bool endFile = false;
    while (!endFile) {
        initializeFrame(frame);
        int res = fread(frame->data, 1, BLOCK_SIZE, f);
        if (ferror(f)) {
            protocolError(s, strerror(errno));
            return;
        }
        if (feof(f)) {
            frame->command = FSR_DATAEND;
            endFile = true;
        } else {
            frame->command = FSR_DATA;
        }
        frame->length = res;
        sendFrame(s, frame);
    }
    fclose(f);
    //... replySuccess(s);
}

static void pwd(int s, ProtocolFrame* frame) 
    throw (ProtocolException) {
    printf("pwd\n");

    char path[PATH_MAX+1];
    initializeFrame(frame);
    if (getcwd(path, PATH_MAX+1) == NULL) {
        protocolError(s, strerror(errno));
        return;
    }
    int len = strlen(path);
    if (len >= FSR_MAXDATALEN) {
        len = FSR_MAXDATALEN - 1;
    }
    frame->command = FSR_DATAEND;
    frame->length = len + 1;
    memmove(frame->data, path, len);
    frame->data[len] = 0;
    sendFrame(s, frame);
}

static void cd(int s, ProtocolFrame* frame) 
    throw (ProtocolException) {
    printf("cd %s\n", frame->data);

    char path[PATH_MAX+1];
    assert(frame->command == FSR_CD);
    int len = frame->length;
    if (len > PATH_MAX)
        len = PATH_MAX;
    memmove(path, frame->data, len);
    path[len] = 0;
    int res = chdir(path);
    if (res == 0) {
        replySuccess(s);
    } else {
        protocolError(s, strerror(errno));
    }
}

static void ls(int s, ProtocolFrame* frame) 
    throw (ProtocolException) {
    printf("ls\n");

    /*...
    char path[PATH_MAX+1];
    if (getcwd(path, PATH_MAX+1) == NULL) {
        protocolError(s, strerror(errno));
        return;
    }
    DIR* dir = opendir(path);
    ...*/

    DIR* dir = opendir(".");
    if (dir == NULL) {
        protocolError(s, strerror(errno));
        return;
    }

    const struct dirent *entry = readdir(dir);
    if (entry == NULL) {
        protocolError(s, strerror(errno));
        return;
    }

    while (entry != NULL) {
        initializeFrame(frame);

        //... frame->data[0] = (char) entry->d_type;
        // The d_type field of struct dirent is not always supperted.
        // So, we use lstat to get information about a file
        struct stat info;
        unsigned char fileType = 0;
        if (lstat(entry->d_name, &info) == 0) {
            if (S_ISDIR(info.st_mode)) {
                fileType = DT_DIR;
            } else if (S_ISLNK(info.st_mode)) {
                fileType = DT_LNK;
            }
        }
        frame->data[0] = (char) fileType;

        /*...
        int len = entry->d_reclen - 
            sizeof(ino_t) - sizeof(off_t) - sizeof(unsigned short);
        ...*/
        int len = strlen(entry->d_name);
        if (len > FSR_MAXDATALEN - 2)
            len = FSR_MAXDATALEN - 2;
        frame->length = len + 2;
        memmove(frame->data + 1, entry->d_name, len + 1);
        frame->data[len + 1] = 0;

        //... printf("%s\n", frame->data + 1);

        entry = readdir(dir);
        if (entry != NULL)
            frame->command = FSR_DATA;
        else
            frame->command = FSR_DATAEND;
        sendFrame(s, frame);
    }
    closedir(dir);
}

static void protocolError(int s, const char *txt)
    throw (ProtocolException) {
    ProtocolFrame frame;
    initializeFrame(&frame);
    frame.command = FSR_ERROR;
    const char *tx = txt;
    char errTxt[64];
    if (tx == NULL) {
        memset(errTxt, 0, 64);
        sprintf(errTxt, "Unknown error %d", s);
        tx = errTxt;
    }
    int len = strlen(tx);
    if (len >= FSR_MAXDATALEN)
        len = FSR_MAXDATALEN - 1;
    frame.length = len + 1;
    memmove(frame.data, tx, len);
    frame.data[len] = 0;
    sendFrame(s, &frame);
}

static void replySuccess(int s)  throw (ProtocolException) {
    ProtocolFrame frame;
    initializeFrame(&frame);
    frame.command = FSR_SUCCESS;
    frame.length = 0;
    sendFrame(s, &frame);
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
