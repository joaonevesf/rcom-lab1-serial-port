// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define TRUE 1
#define FALSE 0

#define FLAG_RCV 0x7E
#define A_RCV 0x03
#define C_SET 0x03
#define C_UA 0x07
#define ESC 0x7D

// GLOBALS
int alarmEnabled = FALSE;
int alarmCount = 0;
int fd = 0;
int frameNumber = 0;
unsigned char escFlag[] = {ESC, 0x5E};
unsigned char escEsc[] = {ESC, 0x5d}; 

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm triggered, #%d\n", alarmCount);
}


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{

    const char *serialPortName = connectionParameters.serialPort;
    LinkLayerRole role = connectionParameters.role;
    int nRetransmissions = connectionParameters.nRetransmissions;
    int timout = connectionParameters.timeout;

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);


    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

// -----------------------------------------------------

    int stop = FALSE;
    enum STATE {START, FLAG, A, C, BCC, STOP};
    unsigned char buf;
    unsigned char received[5] = {0};
    int index = 0;
    enum STATE state = START;
    unsigned char set_command[] = {FLAG_RCV, A_RCV, C_SET, A_RCV ^ C_SET, FLAG_RCV};

    switch (role) {
        case LlTx:

            while(stop == FALSE && nRetransmissions > 0) {

                int byte1 = write(fd, set_command, 5);
                printf("%d bytes written\n", byte1);

                while (stop == FALSE && alarmCount < timout) {
                    if (alarmEnabled == FALSE) {
                        alarm(1); // Set alarm to be triggered in 1s
                        alarmEnabled = TRUE;
                    }

                    int byte = read(fd, &buf, 1);
                    if(byte <= 0) continue;
                    switch(state) {
                        case START: 
                            if(buf == FLAG_RCV) {
                                state = FLAG;
                                received[index] = buf;
                                index+=1;    
                            }
                            break;
                        case FLAG:
                            if(buf == A_RCV) {
                                state = A;
                                received[index] = buf;
                                index+=1;
                            }
                            else if(buf != FLAG_RCV) {
                                state = START;
                                index = 0;
                            } 
                            break;
                        case A: 
                            if(buf == C_UA) {
                                state = C;        
                                received[index] = buf;
                                index+=1;       
                            }
                            else if(buf == FLAG_RCV) {
                                state = FLAG;
                                index = 1;
                            } 
                            else {
                                state = START;
                                index = 0;
                            } 
                            break;
                        case C:
                            if(buf == (received[1] ^ received[2])) {
                                state = BCC;
                                received[index] = buf;
                                index+=1;
                            }
                            else if(buf == FLAG_RCV) {
                                state = FLAG;
                                index = 1;
                            } 
                            else {
                                state = START;
                                index = 0;
                            } 
                            break;
                        case BCC:
                            if(buf == FLAG_RCV) {
                                stop = TRUE;
                                received[index] = buf;
                                index+=1;
                            }
                            else {
                                state = START;
                                index = 0;
                            } 
                            break;
                        default:
                            break; 
                    }
                }
                alarmCount = 0;
                nRetransmissions--;
            }
            if(stop  == FALSE) return -1;
            printf("UA received\n");
            break;
        

        case LlRx:
            
            while (stop == FALSE)
            {
                int byte = read(fd, &buf, 1);
                if(byte <= 0) continue;
                switch(state) {
                    case START: 
                        if(buf == FLAG_RCV) {
                            state = FLAG;
                            received[index] = buf;
                            index+=1;    
                        }
                        break;
                    case FLAG:
                        if(buf == A_RCV) {
                            state = A;
                            received[index] = buf;
                            index+=1;
                        }
                        else if(buf != FLAG_RCV) {
                            state = START;
                            index = 0;
                        } 
                        break;
                    case A: 
                        if(buf == C_SET) {
                            state = C;        
                            received[index] = buf;
                            index+=1;       
                        }
                        else if(buf == FLAG_RCV) {
                            state = FLAG;
                            index = 1;
                        } 
                        else {
                            state = START;
                            index = 0;
                        } 
                        break;
                    case C:
                        if(buf == (received[1] ^ received[2])) {
                            state = BCC;
                            received[index] = buf;
                            index+=1;
                        }
                        else if(buf == FLAG_RCV) {
                            state = FLAG;
                            index = 1;
                        } 
                        else {
                            state = START;
                            index = 0;
                        } 
                        break;
                    case BCC:
                        if(buf == FLAG_RCV) {
                            stop = TRUE;
                            received[index] = buf;
                            index+=1;
                        }
                        else {
                            state = START;
                            index = 0;
                        } 
                        break;
                    default:
                        break; 
                }

            }

            printf("Flag received\n");

            // Create string to send
            unsigned char ua_reply[] = {FLAG_RCV, A_RCV, C_UA, A_RCV ^ C_UA, FLAG_RCV};

            int bytes = write(fd, ua_reply, 5);
            printf("%d bytes written\n", bytes);

            break;


        default:
            break;
    }


// -----------------------------------------------------

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

int writeByte(const unsigned char* byte, unsigned char* buffer, int* idx) {
    if(*byte == FLAG_RCV) {
        memcpy(&buffer[*idx], escFlag, 2);
        *idx = *idx + 2;
    } else if(*byte == ESC) {
        memcpy(&buffer[*idx], escEsc, 2);
        *idx = *idx + 2;
    } else {
        memcpy(&buffer[*idx], byte, 1);
        *idx = *idx + 1;
    }
    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
   

    unsigned char FLAG_C = frameNumber = 0 ? 0x00 : 0x40;
    unsigned char header[] = { FLAG_RCV, A_RCV, FLAG_C, A_RCV ^ FLAG_C};
    unsigned char buffer[bufSize * 2];
    int idx = 5;
    
    memcpy(buffer, header, 4);

    unsigned char bcc2 = buf[0];
    
    writeByte(&buf[0], buffer, &idx);

    int bytesWritten = 1;
    while(bytesWritten < bufSize) {

        bcc2 ^= buf[bytesWritten];
        writeByte(&buf[bytesWritten++], buffer, &idx);

    }

    writeByte(&bcc2, buffer, &idx);
    buffer[idx++] = FLAG_RCV;

    int bytes = write(fd, buffer, idx);
    if(bytes < idx) {
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
