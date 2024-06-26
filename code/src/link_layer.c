// Link layer protocol implementation

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define TRUE 1
#define FALSE 0

#define FLAG_RCV 0x7E
#define A_T 0x03
#define A_R 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define CI_0 0x00
#define CI_1 0x40
#define ESC 0x7D
#define ESC_XOR 0x20
#define RR0 0x05
#define RR1 0x85
#define REJ0 0x01
#define REJ1 0x81


// GLOBALS
int alarmEnabled = FALSE;
int alarmCount = 0;
int fd = 0;
struct termios oldtio; // old settings to restore
int frameNumber = 0;
unsigned char escFlag[] = {ESC, 0x5E};
unsigned char escEsc[] = {ESC, 0x5d}; 
int timout = 0;
int nRetransmissions = 0; 
LinkLayerRole role;


long bytesSent = 0;
long bytesReceived = 0;
int errorsSent = 0;
int errorsReceived = 0;


int DEBUG = FALSE;


int SIM_ERROR = FALSE;
int ERROR_RATE = 10; // % error rate (0-100)


// Alarm function handler
void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    if(DEBUG) printf("Alarm triggered, #%d\n", alarmCount);
}

int cHandler(Action act, unsigned char buf) {
    switch(act) {
        case RCV_SET:
            if(buf == C_SET) return 1;
            break;
        case RCV_UA:
            if(buf == C_UA) return 1;
            break;
        case WRITE:
            if(buf == RR0 || buf == RR1 || buf == REJ0 || buf == REJ1) return 1;
            break;
        case READ:
            if(buf == CI_0 || buf == CI_1) return 1;
        case CLOSETX:
        case CLOSERX:
            if(buf == C_DISC) return 1;
            break;
        default:
            break;
    }
    return 0;
}

int parseFrame(Action act, State* state, unsigned char* received, int* index) {
    int stop = FALSE;
    unsigned char buf;
    int bytes = read(fd, &buf, 1);
    if(bytes < 1) {
        return stop;
    }
    bytesReceived += bytes;
    switch(*state) {
        case START: 
            if(buf == FLAG_RCV) {
                *state = FLAG;
                received[*index] = buf;
                *index+=1;    
            }
            break;
        case FLAG:
            if(buf == A_R && act == CLOSETX) {
                *state = A;
                received[*index] = buf;
                *index+=1;
            }
            else if (buf == A_T && act != CLOSETX) {
                *state = A;
                received[*index] = buf;
                *index+=1;
            }
            else if(buf != FLAG_RCV) {
                *state = START;
                *index = 0;
            } 
            break;
        case A: 
            if(cHandler(act, buf)) {
                *state = C;        
                received[*index] = buf;
                *index+=1;       
            }
            else if(buf == FLAG_RCV) {
                *state = FLAG;
                *index = 1;
            } 
            else {
                *state = START;
                *index = 0;
            } 
            break;
        case C:
            if(buf == (received[1] ^ received[2])) {
                *state = BCC;
                received[*index] = buf;
                *index+=1;
            }
            else if(buf == FLAG_RCV) {
                *state = FLAG;
                *index = 1;
            } 
            else {
                *state = START;
                *index = 0;
            } 
            break;
        case BCC:
            if(buf == FLAG_RCV) {
                stop = TRUE;
                received[*index] = buf;
                *index+=1;
            }
            else {
                *state = START;
                *index = 0;
            } 
            break;
        default:
            break; 
    }
    return stop;
}



////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters) {

    const char *serialPortName = connectionParameters.serialPort;
    role = connectionParameters.role;
    nRetransmissions = connectionParameters.nRetransmissions;
    timout = connectionParameters.timeout;

    (void)signal(SIGALRM, alarmHandler);

    fd = open(serialPortName, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPortName);
        exit(-1);
    }

    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1) {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; 
    newtio.c_cc[VMIN] = 0; 

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

// -----------------------------------------------------

    int stop = FALSE;
    unsigned char received[5] = {0};
    int index = 0;
    State state = START;
    unsigned char set_command[] = {FLAG_RCV, A_T, C_SET, A_T ^ C_SET, FLAG_RCV};
    unsigned char ua_reply[] = {FLAG_RCV, A_T, C_UA, A_T ^ C_UA, FLAG_RCV};
    int nRepeated = 0;
    
    switch (role) {
        case LlTx:

            while(stop == FALSE && nRepeated < nRetransmissions) {

                int byte1 = write(fd, set_command, 5);
                if(byte1 < 5) {
                    printf("Error writing SET\n");
                    return -1;
                }
                bytesSent += byte1;
                if(DEBUG) printf("%d bytes written (SET)\n", byte1);

                while (stop == FALSE && alarmCount < timout) {
                    if (alarmEnabled == FALSE) {
                        alarm(1); // Set alarm to be triggered in 1s
                        alarmEnabled = TRUE;
                    }

                    stop = parseFrame(RCV_UA, &state, received, &index);

                }
                alarmCount = 0;
                nRepeated++;
            }
            if(stop  == FALSE) {
                printf("Error receiving UA\n");
                return -1;
            }
            break;
        

        case LlRx:
            
            while (stop == FALSE) {
                stop = parseFrame(RCV_SET, &state, received, &index);
            }

            if(write(fd, ua_reply, 5) < 5) {
                printf("Error writing UA\n");
                return -1;
            }
            bytesSent += 5;
            break;


        default:
            break;
    }

    return 0;
}

void writeByte(const unsigned char* byte, unsigned char* buffer, int* idx) {
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
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
   
    unsigned char control = frameNumber == 0 ? CI_0 : CI_1;
    unsigned char header[] = { FLAG_RCV, A_T, control, A_T ^ control};
    unsigned char buffer[bufSize * 2];
    int idx = 4;
    
    memcpy(buffer, header, 4);

    unsigned char bcc2 = 0;

    int bytesWritten = 0;
    while(bytesWritten < bufSize) {
        bcc2 ^= buf[bytesWritten];
        writeByte(&buf[bytesWritten++], buffer, &idx);
    }

    writeByte(&bcc2, buffer, &idx);
    buffer[idx++] = FLAG_RCV;
    int bytes;
    int stop = FALSE;
    int good_packet = FALSE;
    int retransmission = TRUE;
    State state = START;
    int nRepeated = 0;
    int error;
    
    while(stop == FALSE && nRepeated <= nRetransmissions) {

        if(retransmission == TRUE) {
            // simulate error
            if(SIM_ERROR) {
                error = rand() % 10000;
                if(error < ERROR_RATE * 100) {
                    if(DEBUG) printf("Simulating error on frame number %d...\n", frameNumber);
                    errorsSent++;
                    buffer[4] = buffer[4] ^ 0xFF; // flips a byte
                }
            }
            bytes = write(fd, buffer, idx);
            nRepeated++;
            if(SIM_ERROR && error < ERROR_RATE * 100) {
                buffer[4] = buffer[4] ^ 0xFF; // undo flip           
            }
            if(bytes < idx) {
                printf("Error writing DATA\n");
                return -1;
            }
            bytesSent += bytes;
        }
        unsigned char received[5] = {0};
        int index = 0;
        good_packet = FALSE;
        state = START;

        while (good_packet == FALSE && alarmCount < timout) {
            if (alarmEnabled == FALSE) {
                alarm(1);
                alarmEnabled = TRUE;
            }
            good_packet = parseFrame(WRITE, &state, received, &index);
        }
        int response_number = received[2] == RR0 ? 0 : 1;
        switch(received[2]){
            case RR0:
            case RR1:
                if(DEBUG) printf("RR%d received\n", response_number);
                if(frameNumber != response_number) {
                    frameNumber = response_number;
                    stop = TRUE;
                }
                else {
                    retransmission = FALSE;
                    nRepeated = 0;
                }
                break;
            case REJ0:
                retransmission = frameNumber == 0 ? TRUE : FALSE;
                if(DEBUG) printf("REJ0 received retransmission: %d\n", retransmission);
                nRepeated = 0;
                break;
            case REJ1:
                retransmission = frameNumber == 1 ? TRUE : FALSE;
                if(DEBUG) printf("REJ1 received, retransmission: %d\n", retransmission);
                nRepeated = 0;
                break;
            default:
                break;
        }
        alarmCount = 0;

    }
    if(nRepeated >= nRetransmissions) {
        printf("Error sending frame due to max number of retransmissions\n");
        return -1;
    }
    return bytes;
}

int sendDataResponse(int valid, unsigned char control) {
    unsigned char responseC;
    int accept = FALSE;
    if(valid) {
        if(control == frameNumber) {
            responseC = frameNumber == 0 ? RR1 : RR0;
            frameNumber ^= 1; 
            accept = TRUE;
        } else {
            responseC = frameNumber == 0 ? RR0 : RR1;
            accept = FALSE;
        }
        if(DEBUG) printf("packet received, RR%d sent\n", responseC == RR0 ? 0 : 1);
    } else {
        if(control == frameNumber) {
            if(DEBUG) printf("error received, REJ%d sent\n", frameNumber);
            responseC = frameNumber == 0 ? REJ0 : REJ1;
            accept = FALSE;
        } else {
            if(DEBUG) printf("error received, RR%d sent\n", frameNumber);
            responseC = frameNumber == 0 ? RR0 : RR1;
            accept = FALSE;
        }
        errorsReceived++;
    }
    unsigned char response[] = {FLAG_RCV, A_T, responseC, A_T ^ responseC, FLAG_RCV};
    int bytes = write(fd, response, 5);
    if(bytes < 5) {
        printf("Error writing response\n");
        return -1;
    }
    bytesSent += bytes;
    if(DEBUG) printf("%d bytes data response written\n", bytes);
    return accept;
}                      
    
////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    
    State state = START;
    int stop = FALSE;
    int index = 0;
    unsigned char bcc2 = 0;
    unsigned char buf;
    unsigned char control;

    while(stop == FALSE) {

        int bytes = read(fd, &buf, 1);
        if(bytes < 1) continue;
        bytesReceived += bytes;

        switch (state) {
            case START: 
                if(buf == FLAG_RCV) {
                    state = FLAG;
                }
                break;
            case FLAG:
                if(buf == A_T) {
                    state = A;
                }
                else if(buf != FLAG_RCV) {
                    state = START;
                } 
                break;
            case A: 
                if(cHandler(READ, buf)) {
                    control = buf;
                    state = C;             
                }
                else if(buf == FLAG_RCV) {
                    state = FLAG;
                } 
                else {
                    state = START; 
                } 
                break;
            case C:
                if(buf == (A_T ^ control)) {
                    state = D;
                }
                else if(buf == FLAG_RCV) {
                    state = FLAG;
                } 
                else {
                    state = START;
                } 
                break;
            case D:
                if(buf == FLAG_RCV) {
                    unsigned char bcc2Received = packet[--index];
                    bcc2 ^= bcc2Received;
                    packet[index] = '\0';
                    int accept = sendDataResponse(bcc2Received == bcc2, control == CI_0 ? 0 : 1);
                    if(accept == -1) {
                        return -1;
                    }
                    if(accept == TRUE) {
                        stop = TRUE;
                    }
                    else {
                        state = START;
                        index = 0;
                        bcc2 = 0;
                    }   
                } else if (buf == ESC) {
                    state = DD;
                } else {
                    packet[index++] = buf;
                    bcc2 ^= buf;
                }
                break;
            case DD:
                packet[index++] = buf ^ ESC_XOR;
                bcc2 ^= buf ^ ESC_XOR;
                state = D;
                break;
            default:
                break; 
        }

    }

    return index;
}

int sendDISC() {
    unsigned char disc[] = {FLAG_RCV, role == LlTx ? A_T : A_R, C_DISC,(role == LlTx ? A_T : A_R) ^ C_DISC, FLAG_RCV};
    int bytes = write(fd, disc, 5);
    bytesSent += bytes;
    if(bytes < 5) {
        printf("Error writing DISC\n");
        return -1;
    }
    if(DEBUG) printf("%d bytes DISC written\n", bytes);
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    int stop = FALSE;
    State state = START;
    int index = 0;
    unsigned char received[5] = {0};
    unsigned char ua_reply[] = {FLAG_RCV, A_T, C_UA, A_T ^ C_UA, FLAG_RCV};
    
    switch (role) {
        case LlTx:
            while(stop == FALSE) {
                if(sendDISC() == -1) {
                    printf("Error sending DISC\n");
                    return -1;
                }
                while(stop == FALSE && alarmCount < timout) {
                    if (alarmEnabled == FALSE) {
                        alarm(1); // Set alarm to be triggered in 1s
                        alarmEnabled = TRUE;
                    }
                    stop = parseFrame(CLOSETX, &state, received, &index);
                }
                
                alarmCount = 0;
            }
            if(stop == TRUE){
                if(DEBUG) printf("DISC received\n");
                if(write(fd, ua_reply, 5) < 5) {
                    printf("Error writing UA\n");
                    return -1;
                }
                bytesSent += 5;
            }
            break;
        case LlRx:  
            while (stop == FALSE) {
                stop = parseFrame(CLOSERX, &state, received, &index);
            }
            stop = FALSE;
            while (stop == FALSE)
            {
                if(sendDISC() == -1) {
                    printf("Error sending DISC\n");
                    return -1;
                }
                while (stop == FALSE && alarmCount < timout) {
                    if (alarmEnabled == FALSE) {
                        alarm(1); // Set alarm to be triggered in 1s
                        alarmEnabled = TRUE;
                    }
                    stop = parseFrame(RCV_UA, &state, received, &index); // RECEIVES UA
                }
                alarmCount = 0;
            }
            break;
        default:
            break;
    }


    if(showStatistics) {
        printf("Error frames sent: %d\n", errorsSent);
        printf("Error frames received: %d\n", errorsReceived);
        printf("Total Bytes Sent: %ld\n", bytesSent);
        printf("Total Bytes Received: %ld\n", bytesReceived);
    }

    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
        }

    close(fd);
    
    return 0;
}
