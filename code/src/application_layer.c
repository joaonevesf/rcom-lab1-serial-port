// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>

#define PACKET_SIZE 256
#define CONTROL_DATA 0x01
#define CONTROL_START 0x02
#define CONTROL_END 0x03
#define FILE_SIZE_T 0x00
#define FILE_NAME_T 0x01

extern int DEBUG;

int applicationWrite(const char *filename) {
    FILE *file = fopen(filename, "r");

    if(file == NULL) {
        printf("Failed to open file\n");
        return 1;
    }

    fseek(file, 0, SEEK_END);

    long fileSize = ftell(file);

    fseek(file, 0, SEEK_SET);

    long nPackets = ((fileSize + PACKET_SIZE) / PACKET_SIZE);
    int nameSize = strlen(filename);
    // Control Packet -> 0x02 / 0x00 / size of fileSize / fileSize (/ 0x01 / size of filename/ filename)
    int fileSizeBytes = 0;
    long aux = fileSize;
    while(aux > 0) {
        aux = aux >> 8;
        fileSizeBytes++;
    }

    unsigned char* controlPacket = malloc(5 + fileSizeBytes + nameSize);
    controlPacket[0] = CONTROL_START;
    controlPacket[1] = FILE_SIZE_T;
    controlPacket[2] = fileSizeBytes;
    aux = fileSize; int i = 0;
    while(i < fileSizeBytes) {
        controlPacket[3 + i] = aux & 0xFF;
        aux = aux >> 8;
        i++;
    }
    controlPacket[fileSizeBytes + 3] = FILE_NAME_T;
    controlPacket[fileSizeBytes + 4] = nameSize;
    for(int i = 0; i < nameSize; i++) {
        controlPacket[fileSizeBytes + 5 + i] = filename[i];
    }
    
    if(DEBUG){
        printf("Printing control packet:\n");
        for(int i = 0; i < 5 + fileSizeBytes + nameSize; i++) {
            printf("0x%x ", controlPacket[i]);
        }
        printf("\n");  
    }
    if(llwrite(controlPacket, 5 + fileSizeBytes + nameSize) < 5 + fileSizeBytes + nameSize) {
        printf("Failed to send control packet\n");
        return 1;
    }

    controlPacket[0] = CONTROL_END;
//------------------------------------------------------
    
    // Data Packet -> 0x01 / byte 1 of nº of bytes / byte 2 of nº of bytes / packets...
    unsigned char dataPacket [PACKET_SIZE + 3];
    
    dataPacket[0] = CONTROL_DATA;
    dataPacket[1] = (PACKET_SIZE >> 8) & 0xFF;
    dataPacket[2] = PACKET_SIZE & 0xFF;

    long nPacket = 0;

    while(nPackets--) {
        for(int i = 0; i < PACKET_SIZE; i++) {
            int byte = fgetc(file);
            dataPacket[3 + i] = (unsigned char) byte;
            if(byte == EOF) {
                dataPacket[1] = (i >> 8) & 0xFF;
                dataPacket[2] = i & 0xFF;
                if(llwrite(dataPacket, 3 + i) < 3 + i) {
                    printf("Failed to send data packet\n");
                    return 1;
                }
                if(llwrite(controlPacket, 5 + fileSizeBytes + nameSize) < 5 + fileSizeBytes + nameSize) {
                    printf("Failed to send control packet\n");
                    return 1;
                }
                free(controlPacket);
                fclose(file);
                return 0;
            }
        }
        if(llwrite(dataPacket, PACKET_SIZE + 3) < PACKET_SIZE + 3) {
            printf("Failed to send data packet\n");
            return 1;
        }
        if(DEBUG) printf("Data packet %ld \n", nPacket);

        nPacket++;
    }
    
    if(llwrite(controlPacket, 5 + fileSizeBytes + nameSize) < 5 + fileSizeBytes + nameSize) {
            printf("Failed to send control packet\n");
            return 1;
    }
    fclose(file);
    free(controlPacket);
    return 0;
}

int applicationRead(const char *filename) {
    FILE *file = fopen(filename, "w");

    if(file == NULL) {
        printf("Failed to open file\n");
        return 1;
    }

    unsigned char controlPacket[517] = {0}; // 5 + 2⁸ * 2

    int bytes = llread(controlPacket);
    if(bytes < 7) {
        printf("Failed to receive control packet\n");
        return 1;   
    }

    if(DEBUG){
        printf("Printing control packet:\n");
        for(int i = 0; i < bytes; i++) {
            printf("0x%x ", controlPacket[i]);
        }
        printf("\n");}  

    if(controlPacket[0] != CONTROL_START) {
        printf("Invalid control packet: Start was 0x%x\n", controlPacket[0]);
        return 1;
    }

    long fileSize = 0;
    
    if(controlPacket[1] != FILE_SIZE_T) {
        printf("Invalid control packet: File size type was 0x%x\n", controlPacket[1]);
        return 1;
    }

    for(int i = 0; i < controlPacket[2]; i++) {
        fileSize += (controlPacket[3 + i] << (8*i));
    }
    if(DEBUG) printf("filesize: %ld\n",fileSize);

    if(controlPacket[3 + controlPacket[2]] != FILE_NAME_T) {
        printf("Invalid control packet: File name type was 0x%x\n", controlPacket[3 + controlPacket[2]]);
        return 1;
    }

    int nameSize = controlPacket[4 + controlPacket[2]];
    char* name = malloc(nameSize);
    for(int i = 0; i < nameSize; i++) {
        name[i] = controlPacket[5 + controlPacket[2] + i];
    }
    
    long nPackets = ((fileSize + PACKET_SIZE) / PACKET_SIZE);
    long aux = nPackets;
    if(DEBUG) printf("nPackets: %ld\n", nPackets);

    unsigned char dataPacket [PACKET_SIZE + 3 +1]; 
    
    while(nPackets--) {
        if(llread(dataPacket) < 3) {
            
            printf("Failed to receive data packet\n");
            return 1;
        }

        if(dataPacket[0] != CONTROL_DATA) {
            printf("Invalid data packet, byte 0:%x\n", dataPacket[0]);
            return 1;
        }
        int packetSize = (dataPacket[1] << 8) + dataPacket[2];
        
        if(DEBUG) printf("Data packet %ld\n", aux - nPackets - 1);

        fwrite(dataPacket + 3, 1, packetSize, file);
        
    }

    memset(controlPacket, 0, 517);

    if(llread(controlPacket) < 7) {
        printf("Failed to receive control packet\n");
        return 1;
    }

    if(controlPacket[0] != CONTROL_END) {
        printf("Invalid control packet. End was 0x%x\n", controlPacket[0]);
        return 1;
    }

    if(controlPacket[1] != FILE_SIZE_T) {
        printf("Invalid control packet- File size t was 0x%x\n", controlPacket[1]);
        return 1;
    }

    long fileSize2 = 0;
    for(int i = 0; i < controlPacket[2]; i++) {
        fileSize2 += (controlPacket[3 + i] << (8*i));
    }

    if(fileSize != fileSize2) {
        printf("FileSize does not match\n");
        return 1;
    }

    if(controlPacket[3 + controlPacket[2]] != FILE_NAME_T) {
        printf("Invalid control packet\n");
        return 1;
    }

    int nameSize2 = controlPacket[4 + controlPacket[2]];

    if(nameSize != nameSize2) {
        printf("NameSize does not match\n");
        return 1;
    }

    for(int i = 0; i < nameSize; i++) {
        if(name[i] != controlPacket[5 + controlPacket[2] + i]) {
            printf("Name does not match\n");
            return 1;
        }
    }

    if(DEBUG) printf("\nFile with name %s and size %ld received and named %s\n", name, fileSize, filename);

    fclose(file);

    free(name);

    return 0;
}



void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
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

    if(llopen(connectionParameters)) {
        printf("Failed to open connection\n");
        return;
    }
    
    switch (connectionParameters.role) {
        case LlTx:
            applicationWrite(filename);
            break;
        
        case LlRx:
            applicationRead(filename);
            break;

        default:
            break;
    }

    if(llclose(TRUE)) {
        printf("Failed to close connection\n"); 
        return;
    }

    return;

}
