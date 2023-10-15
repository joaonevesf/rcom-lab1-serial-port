// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // TODO
    unsigned char packet[9] = {0};
    unsigned char packet2[9] = {0};
    unsigned char buff[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    LinkLayer connectionParameters;
    strcpy(connectionParameters.serialPort, serialPort);
    if(strcmp(role, "rx") == 0)
        connectionParameters.role = LlRx;
    else if(strcmp(role, "tx") == 0)
        connectionParameters.role = LlTx;
    else return;
    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;

    if(llopen(connectionParameters)) {printf("Failed to open connection\n"); return;}
    
    switch (connectionParameters.role)
    {
    case LlTx:
        if(llwrite(buff, 8) < 8) {printf("Failed to write\n"); return;}
        if(llwrite(buff, 8) < 8) {printf("Failed to write\n"); return;}
        break;
    
    case LlRx:
        if(llread(packet) < 8) {printf("Failed to read packet\n"); return;}
        if(llread(packet2) < 8) {printf("Failed to read packet\n"); return;}
        break;

    default:
        break;
    }

    if(llclose(0)) {printf("Failed to close connection\n"); return;}

    for(int i = 0; i < 8; i++)
        printf("%x ", packet[i]);
    printf("\n");
    for(int i = 0; i < 8; i++)
        printf("%x ", packet2[i]);

}
