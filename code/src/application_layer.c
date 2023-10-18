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

/*
packet size = 256 bytes?

C = 1 -> data packet
C = 2 -> start packet
C = 3 -> end packet
L2*256 + L1 -> number of bytes
P -> packets

for(...n) data / n -> data packet

data packet = {C, L2, L1, P1, P2, ..., Pn}

llwrite(data packet, 3 + L2*256 + L1);

llread(data packet);

interpretar com switch case 


T = 0 -> file size
T = 1 -> file name

L -> number of bytes of V
V -> value

control packet = {C, T1, L1, V1, T2, L2, V2}

llwrite(control packet, 5 + L1 + L2);

llread(data packet);

interpretar com switch case 


Ler ficheiro
Dividir ficheiro em packets
llopen()
llwrite(control packet) -> Start
for(i to n) llwrite(data packet)
llwrite(control packet) -> End
llclose()
---------------------------------
llopen()
llread(control packet) -> Start
Criar espaço para ficheiro
for(i to n) llread(data packet)
llread(control packet) -> End
llclose()

*/

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
    
    printf("Printing control packet:\n");
    for(int i = 0; i < 5 + fileSizeBytes + nameSize; i++) {
        printf("0x%x ", controlPacket[i]);
    }
    printf("\n");  

    if(llwrite(controlPacket, 5 + fileSizeBytes + nameSize) < 5 + fileSizeBytes + nameSize) {
        printf("Failed to send control packet\n");
        return 1;
    }

    controlPacket[0] = CONTROL_END;
//------------------------------------------------------
    
    // Data Packet -> 0x01 / byte 1 of nº of bytes / byte 2 of nº of bytes / packets...
    unsigned char* dataPacket = malloc(PACKET_SIZE + 3);
    
    dataPacket[0] = CONTROL_DATA;
    dataPacket[1] = (PACKET_SIZE >> 8) & 0xFF;
    dataPacket[2] = PACKET_SIZE & 0xFF;

    long nPacket = 0;

    while(nPackets--) {
        for(int i = 0; i < PACKET_SIZE; i++) {
            dataPacket[3 + i] = fgetc(file);
            if(dataPacket[3 + i] == EOF || dataPacket[3 + i] == 0xFF) {
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
                free(dataPacket);
                fclose(file);
                return 0;
            }
        }
        if(llwrite(dataPacket, PACKET_SIZE + 3) < PACKET_SIZE + 3) {
            printf("Failed to send data packet\n");
            return 1;
        }
        // print data packet
        printf("Printing data packet %ld:\n", nPacket);
        for(int i = 0; i < PACKET_SIZE + 3; i++) {
            printf("0x%x ", dataPacket[i]);
        }
        printf("\n");

        nPacket++;
    }
    
    if(llwrite(controlPacket, 5 + fileSizeBytes + nameSize) < 5 + fileSizeBytes + nameSize) {
            printf("Failed to send control packet\n");
            return 1;
    }
    fclose(file);
    free(controlPacket);
    free(dataPacket);
    return 0;
}

int applicationRead(const char *filename) {
    FILE *file = fopen(filename, "w");

    if(file == NULL) {
        printf("Failed to open file\n");
        return 1;
    }

    unsigned char* controlPacket = malloc(500);
    memset(controlPacket, 0, 500);

    int bytes = llread(controlPacket);
    if(bytes < 7) {
        printf("Failed to receive control packet\n");
        return 1;
    }

    printf("Printing control packet:\n");
    for(int i = 0; i < bytes; i++) {
        printf("0x%x ", controlPacket[i]);
    }
    printf("\n");  

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
        fileSize = fileSize << 8;
        fileSize += controlPacket[3 + i];
    }

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
    printf("nPackets: %ld\n", nPackets);

    unsigned char* dataPacket = malloc(PACKET_SIZE + 3 +1); 
    
    while(nPackets--) {
        if(llread(dataPacket) < 3) {
            
            printf("Failed to receive data packet\n");
            return 1;
        }

        if(dataPacket[0] != CONTROL_DATA) {
            printf("Invalid data packet\n");
            return 1;
        }
        int packetSize = (dataPacket[1] << 8) + dataPacket[2];

        // print data packet
        printf("Printing data packet %ld:\n", aux - nPackets - 1);
        for(int i = 0; i < packetSize + 3; i++) {
            printf("0x%x ", dataPacket[i]);
        }
        printf("\n");

        for(int i = 0; i < packetSize; i++) {
            fputc(dataPacket[3 + i], file);
        }
    }

    memset(controlPacket, 0, 500);

    if(llread(controlPacket) < 7) {
        printf("Failed to receive control packet\n");
        return 1;
    }

    if(controlPacket[0] != CONTROL_END) {
        printf("Invalid control packet. End was 0x%x\n", controlPacket[0]);
        return 1;
    }

    if(controlPacket[1] != FILE_SIZE_T) {
        printf("Invalid control packet- Filze size t was 0x%x\n", controlPacket[1]);
        return 1;
    }

    long fileSize2 = 0;
    for(int i = 0; i < controlPacket[2]; i++) {
        fileSize2 = fileSize2 << 8;
        fileSize2 += controlPacket[3 + i];
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

    printf("\nFile with name %s and size %ld received to %s\n", name, fileSize, filename);

    fclose(file);

    free(name);
    free(dataPacket);
    free(controlPacket);

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
        
        case LlRx:
            applicationRead(filename);

        default:
            break;
    }

    if(llclose(0)) {
        printf("Failed to close connection\n"); 
        return;
    }

    return;

}
