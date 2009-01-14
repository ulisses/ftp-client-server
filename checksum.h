#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char *addCksum(unsigned int chksum, char *msg) {
        char *msgCk = (char *)malloc(sizeof(char)*strlen(msg)+4);

        sprintf(msgCk,"%x",(int)chksum);
        msg = strcat(msg,msgCk);

        return msg;
}

int checksum(char *msg) {
        int  sum, i, n = strlen(msg);

        for(sum = 0, i = 0; i < n; i+=2) {
                sum += msg[i]*256 + msg[i+1];
        }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += sum >> 16;

    return ((unsigned short) ~sum);
}
