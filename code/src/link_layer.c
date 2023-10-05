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

// GLOBALS
int alarmEnabled = FALSE;
int alarmCount = 0;

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
    // TODO

    const char *serialPortName = connectionParameters.serialPort;
    LinkLayerRole role = connectionParameters.role;
    int nRetransmissions = connectionParameters.nRetransmissions;
    int timout = connectionParameters.timeout;

    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);


    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

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

    switch (role) {
        case LlTx:

            unsigned char set_command[] = {FLAG_RCV, A_RCV, C_SET, A_RCV ^ C_SET, FLAG_RCV};

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

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

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
