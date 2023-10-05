// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // TODO

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

    llopen(connectionParameters);
}
