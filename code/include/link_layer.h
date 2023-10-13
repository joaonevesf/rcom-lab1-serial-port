// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>


typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef enum 
{   
    START, 
    FLAG, 
    A, 
    C,
    D,
    DD, 
    BCC,
    BCC2,
    STOP
} State;

typedef enum 
{
    OPENTX, 
    OPENRX, 
    WRITE, 
    READ, 
    CLOSETX,
    CLOSERX
} Action;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;

// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

// Handles the control field (buf) of a packet, depending on the act.
// Return "1" on good field, "0" otherwise.
int cHandler(Action act, unsigned char buf);

// Parses a frame (control and supervision), writing it to received and updating index and state, depending on the act.
// Return "1" on a complete packet, "0" otherwise
int parseFrame(Action act, State* state, unsigned char* received, int* index);

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Writes byte to buffer, escaping FLAG and ESC. Updates index to last open slot.
void writeByte(const unsigned char* byte, unsigned char* buffer, int* idx);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Send data response depending on valid packet and control field.
// Return "1" to save packet, "0" to discard it and "-1" on write fail. 
int sendDataResponse(int valid, unsigned char control);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

#endif // _LINK_LAYER_H_
