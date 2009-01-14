#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>


#define FL_SYN 0x01
#define FL_ACK 0x02
#define FL_FIN 0x04
#define FL_GET 0x08
#define FL_ERRO 0x10
#define FL_DIR 0x20
#define FL_PUT 0x40

#define TAM 200

typedef struct segmento{
	int seq;
	int ack;
	int len;
	int win_size;
	int flags;
	int cksum;
	char data[TAM];
}Datagrama;



typedef struct con_estado{
	int clientBuffer;
	struct timeval rtt; 
}ConState;

void aumentaTimeout(struct timeval *t){
	
}

Datagrama *initDatagrama(){
	Datagrama *novo = (Datagrama *)malloc(sizeof(struct segmento));
	novo->ack = novo->seq = novo->win_size = novo->flags = novo->len = novo->cksum = 0;
	strcpy(novo->data, "c");
	return novo;
}

Datagrama *demuxDatagrama(char *buffer){
	int res;
	Datagrama *novo = (Datagrama *) malloc(sizeof(struct segmento));
	res = sscanf(buffer, "%d %d %d %d %d %d %[^ ]", &novo->seq, &novo->ack, &novo->len, &novo->win_size, &novo->flags, &novo->cksum,novo->data);
	if(res< 7){ 
		free(novo); 
		return NULL;
	}
	else {
		int i, diff = (strlen(buffer)) - novo->len;
		for(i = 0; i < novo->len && i< TAM; i++) novo->data[i] = buffer[diff + i];
		novo->data[i] = '\0';
		return novo;
	}
}

char *muxDatagrama(Datagrama *novo){
	if(novo){
	char *buffer = (char *)malloc(sizeof(char)*(TAM+24));
	sprintf(buffer, "%d %d %d %d %d %d %s", novo->seq, novo->ack, novo->len, novo->win_size, novo->flags, novo->cksum, novo->data);
	return buffer;
	}
	else return NULL;
}

void mostraFlagsActivas(int flags){
	if(flags & FL_SYN) printf("FL_SYN\n");
	if(flags & FL_ACK) printf("FL_ACK\n");
	if(flags & FL_FIN) printf("FL_FIN\n");
	if(flags & FL_GET) printf("FL_GET\n");
	if(flags & FL_ERRO) printf("FL_ERRO\n");
}


void mostraDatagrama(Datagrama *data){
if(data){
	printf("Ack: %d\n", data->ack);
	printf("Seq: %d\n", data->seq);
	printf("Len: %d\n", data->len);
	printf("Win_size: %d\n", data->win_size);
	printf("checksum: %d\n", data->cksum);
	mostraFlagsActivas(data->flags);
	printf("Data:%s", data->data);
	}
	else printf("Formato desconhecido");
}

/*Vai ler no máximo nbytes. retorna o nº de bytes lidos*/
int readFile(int file, int nbytes, char *buffer){
	int n, i = 0;
	n = 1;
	while(i < nbytes && n > 0){
	n = read(file, &(buffer[i]), 1);
	i++;
	}
	buffer[i] = '\0';
	return i;
}


Datagrama *parteFicheiro(int fich, int nbytes, int seq){
	int length, bytesRead = 0;
	Datagrama *novo = initDatagrama();
	
	if(nbytes < TAM){
		length = nbytes;
	}else length = TAM;
		
	bytesRead = readFile(fich, length, novo->data);
	
	if(bytesRead == length){
	//numero de bytes lidos
		novo->seq = seq + bytesRead;
		novo->ack = 0;
		novo->len = strlen(novo->data);
		novo->win_size = 1;
		return novo;
	}
	else {
		novo->seq = seq + bytesRead;
		if(strlen(novo->data)==1) strcpy(novo->data,"c");
		novo->len = strlen(novo->data);
		novo->flags = FL_FIN;
		return novo;
		}
			
}






