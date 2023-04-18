//
// File "filesrv.h"
// Test file server
// Protocol definitions
//
#ifndef TST_SRV_H
#define TST_SRV_H

const int FSR_MAXDATALEN = 2048;

// Protocol frame
struct ProtocolFrame {
    int command;                // Protocol action
    int length;                 // Length of data field
    char data[FSR_MAXDATALEN];  // Various length field
};

const int FSR_HEADERLEN = 2 * sizeof(int); // Length of frame header without data

// Protocol commands
const int FSR_SUCCESS = 0;  // Command is performed successfully
const int FSR_ERROR = 1;    // Cannot perform a command
const int FSR_READY = 2;    // Server is ready
const int FSR_DATA = 3;     // Block of data, next block follows
const int FSR_DATAEND = 4;  // Last block of data
const int FSR_PUT = 5;      // Put a file
const int FSR_GET = 6;      // Get a file
const int FSR_PWD = 7;      // Pring Work Directory
const int FSR_CD = 8;       // Change directory
const int FSR_LS = 9;       // List a directory contents
const int FSR_LOGOUT = 10;  // End a session

// Data returned in response to FSR_LS command
// contains a directory entry of type
//     struct dirent
// that contains one entry in a directory

#endif
