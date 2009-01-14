#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include "checksum.h"
#include "geradorErro.h"
#include "datagrama.h"
#include <fcntl.h>

#define TAM2 256
#define PORT 9999

int sock, length, n, portno=PORT;
struct sockaddr_in serv_addr, from;
struct hostent *server;

typedef struct flag {
        int bool;
        int pos;
}Flag;


int sendMsg(int te, int tp, int sock, char *buffer, ConState *con, int lastSeq, struct sockaddr_in *from, int *fromlen){
	int r, n, send = 0, try = 0;
	char *msg, *cpy;
	char rec[TAM2];
	Datagrama *recebido;
	struct timeval t;
	fd_set fds;	
	
	FD_ZERO(&fds); FD_SET(sock, &fds); //inicializa fds e coloca sock em fds
	t.tv_sec = con->rtt.tv_sec +1 ; t.tv_usec = con->rtt.tv_usec; //inicializa o timeval
	
	while(!send && try < 3){
		cpy = strdup(buffer); //cria uma cópia para k nao se perca o segmento sem erros
		msg = insereErro(te, tp, cpy); //retorna NULL em caso de perda ou uma mensagem com/sem erros
		
		
		if(msg){//Pode conter erros mas não existe perda
			
			n = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *)from, *fromlen);
			if(n < 0){
				perror("sendto");
			}
			
		/*Espera um ACK*/
			r = select(sock + 1, &fds, NULL, NULL, &t);
			
			if(r && FD_ISSET(sock, &fds)){ //Foi escrito algo no socket
				n = recvfrom(sock, rec, TAM2,0 , (struct sockaddr *)from, (socklen_t*) fromlen);
				if(n < 0){
					perror("recvfrom");
					exit(0);
				}
				rec[n] = '\0';
				/*Verificar ack recebido e tomar decisao*/
				recebido = demuxDatagrama(rec);
				if(recebido && recebido->ack > lastSeq){ //segmento foi enviado sem erros
					send = 1;
					free(recebido);
				}
			}
			else{//Ocorreu uma situação de timeout
			 	try++;
			 	printf("Ocorreu um erro\n");
			 	}
			free(msg);
		}
		else {
			printf("Ocorreu uma perda merda!!!....\n");
		}
		free(cpy);
	}
	if(!send) return 0;
	else return 1;
}

/*
Versão inicial que transmite um ficheiro.
	- Só faz retransmite se receber um ack duplicado (ack indica qual o inicio dos proximos bytes que om cliente fica á espera)
*/
int transfereFich(int te, int tp, int sock, char *file, int destBuffer, ConState *con ,struct sockaddr_in *from, int fromlen){
	int trans, f, fin,  seq = 0;
	char *buffer;
	Datagrama *enviar;
	if((f = open(file, O_RDONLY))< 0){ //Ocorreu um erro na abertura do ficheiro. Possivelmente não existe
		enviar = initDatagrama();
		enviar->flags = (FL_ERRO | FL_FIN);
		buffer = muxDatagrama(enviar);
		free(enviar);
		sendto(sock,buffer,strlen(buffer), 0, (struct sockaddr *)from, fromlen);
		free(buffer);
		perror("open");
		return -1;
	}
	else {
		fin  = 0;
		trans = 1;
		while(!fin && trans){
			enviar = parteFicheiro(f, destBuffer, seq); //Segmento com atenção ao buffer do cliente
			enviar->cksum = checksum(enviar->data);     //Calcula o cksum da informação e junta ao segmento
			seq = enviar->seq;
			
			fin = (enviar->flags & FL_FIN);
			buffer = muxDatagrama(enviar);
			
			trans = sendMsg(te, tp, sock, buffer, con, seq, from, &fromlen);
			free(enviar);
			free(buffer);
		}
	return fin;
	}
}


/*Funçao se não receber informação durante 30 segundos para a execução e elimina tudo o que tinha feito*/
int recebeFicheiro(FILE *f, int sock, struct sockaddr_in *from, int fromlen){
	int r, try = 0, cksum, n, lastAck, fin = 0;
	Datagrama *receber, *enviar;
	char *buffer, *res = NULL;
	int timeout = 10;
	struct timeval t;
	fd_set fds;
	lastAck = 0;
	
	FD_ZERO(&fds); FD_SET(sock, &fds); //inicializa fds e coloca sock em fds
	t.tv_sec = timeout; t.tv_usec = 0; //inicializa o timeval
	
	while(!fin && try < 3){
	
		r = select(sock + 1, &fds, NULL, NULL, &t);
		
		if(r && FD_ISSET(sock, &fds)){
			buffer = (char *)malloc(sizeof(char)*TAM2);
			n = recvfrom(sock, buffer, TAM2, 0, (struct sockaddr *)from, (socklen_t *) &fromlen);
			if(n < 0){
				perror("recvfrom");
				exit(1);
				}
			buffer[n] = '\0';
			receber = demuxDatagrama(buffer);
			if(receber && !(receber->flags & FL_ERRO)){
				fin = receber->flags & FL_FIN;
				cksum = checksum(receber->data); //calcular checksum do segmento rebido
				enviar = initDatagrama();
				enviar->flags = FL_ACK;
			
				if(cksum!=receber->cksum){//Informação foi alterada na tranferência do ficheiro.
					printf("Falha: %s", receber->data);
				}
				else {//Informação livre de erros. Enviar ack com o inicio do proximo cnojunto de bytes a receber
					lastAck = receber->seq + 1;
					if(receber->len > 1 && f!=NULL) fprintf(f, "%s", receber->data);
					else printf("%s", receber->data);
				}
				enviar->ack = lastAck;
				res = muxDatagrama(enviar);
				n = sendto(sock, res, strlen(res), 0, (struct sockaddr *)from, (socklen_t) fromlen);
				if(n < 0){
					perror("sendto");
					exit(1);
					}
				//free(res);
				free(enviar);
				free(receber);
			}else {//Poderá ter ocorrido corrupção dos dados e o demuxDatagrama nao conhecer o formato do segmento
				//enviar ack com a sequencia de bytes esperada
				enviar = initDatagrama();
				enviar->flags = FL_ACK;
				enviar->ack = lastAck;
				res = muxDatagrama(enviar);
				n = sendto(sock, res, strlen(res), 0, (struct sockaddr *)from, (socklen_t) fromlen);
				if(n < 0){
					perror("sendto");
					exit(1);
					}
				free(enviar);
				printf("Falhou: %s", buffer);
				fin = 1;
			}
			free(buffer);
			}
		else {//Não leu nada ao fim de 10 segundos
			try++;
		}
	}
	if(fin) return 1; else return 0;
}

ConState* conecta(int sock){
	char *buffer;
	int res, n, try, synACK = 0;
	try = 0;
	ConState *con = (ConState *)malloc(sizeof(struct con_estado));
	//Variaveis necessarias para o select
	int timeout = 10;
	struct timeval t;
	fd_set fds;
	
	Datagrama *novo = initDatagrama();
	Datagrama *recebido;
	novo->win_size = TAM; //Aquando da conexão o campo win_size irá indicar ao servidor o tamanho do buffer
	novo->flags = FL_SYN;
	buffer = muxDatagrama(novo); //buffer contem o pedido de ligação
	
        
	//Usar o select para usar uma estratégia não bloqueante	
	//Envia pedido de ligação ao servidor
	n = sendto(sock,buffer, strlen(buffer),0,(struct sockaddr *)&serv_addr,(socklen_t)length);
        if (n < 0){
        	perror("Sendto");
        	exit(1);
        	}
        
        FD_ZERO(&fds); FD_SET(sock, &fds); //inicializa fds e coloca sock em fds
	t.tv_sec = timeout; t.tv_usec = 0; //inicializa o timeval
        

        while(try<10 && !synACK){
        	
		res = select(sock + 1, &fds, NULL, NULL, &t);
        	if(res < 0){
        		perror("select");
        		exit(0);
        	}
        	
        	if(res && FD_ISSET(sock, &fds)){ //Verifica se o sock está em fds 
        		n = recvfrom(sock,buffer,TAM2,0,(struct sockaddr *)&from,(socklen_t*) &length);
       	 		if (n < 0){ //Não foi lido nada
       	 		perror("recvFrom");
       	 		//envia novamente buffer (pedido de ligação)
       	 		n = sendto(sock,buffer, strlen(buffer),0,(struct sockaddr *)&serv_addr,(socklen_t)length);
        		if (n < 0){
        			perror("sendto");
        			exit(1);
        		}
        		try++;
       	 		}
        		else {
        		//Obteve resposta
        		buffer[n] = '\0';
        		recebido = demuxDatagrama(buffer);
        		if(recebido && (recebido->flags & FL_SYN) && (recebido->flags & FL_ACK)){
        			/*Recebeu a confirmação para a ligação*/
        			con->clientBuffer = recebido->win_size;
				con->rtt.tv_sec = (timeout - t.tv_sec);
				con->rtt.tv_usec = t.tv_usec;
        			synACK = 1;
        			free(buffer);
        			free(recebido);
        			//envia ack para confirmar ligação e tamanho do buffer em novo->len
        			novo->flags = 0;
        			novo->win_size = TAM;
        			novo->flags = (FL_ACK);
        			buffer = muxDatagrama(novo);
        			sleep(1);
        			n = sendto(sock,buffer, strlen(buffer),0,(struct sockaddr *)&from,(socklen_t)length);
        			if (n < 0){
					perror("Sendto");
					exit(1);
        			}
        			}
        		else {
        			//Recebeu confirmação mas não aceita ligação
        			free(buffer);
        			free(recebido);
        			}
        		}
        	}
        	try++;
        }
	if(FD_ISSET(sock, &fds)) FD_CLR(sock, &fds);
	if(!synACK) return NULL;
	else return con;
}

Datagrama* trataPedido(char *pedido){
	if(strstr(pedido, "dir") && strstr(pedido, "get")==NULL){
	Datagrama *novo = initDatagrama();
	novo->flags = (FL_DIR);
	//strcpy(novo->data, "dir.cmd");
	novo->len = strlen("dir.cmd");
	novo->win_size = 1;
	return novo;
	}
	if(strstr(pedido, "get")){
	Datagrama *novo = initDatagrama();
	char ptr[5];
	novo->flags = (FL_GET);
	sscanf(pedido, "%s %s", ptr, novo->data);
	novo->len = strlen(novo->data);
	novo->win_size = 1;
	return novo;
	}
	if(strstr(pedido, "wait")){
	Datagrama *novo = initDatagrama();
	novo->flags = (FL_GET);
	strcpy(novo->data, "wait");
	novo->len = strlen(novo->data);
	novo->win_size = 1;
	return novo;
	}
	if(strstr(pedido, "put")){
	Datagrama *novo = initDatagrama();
	char ptr[5];
	novo->flags = (FL_PUT);
	sscanf(pedido, "%s %s", ptr, novo->data);
	novo->len = strlen(novo->data);
	novo->win_size = 1;
	return novo;
	}
	
	else return NULL;
}


int enviaPedido(int sock, char *buffer, struct sockaddr_in *servidor,int te, int tp){
	if(!buffer){
		printf("erro no comando\n");
		return 0;
	}else {
		n = sendto(sock,buffer, strlen(buffer),0,(struct sockaddr *)servidor ,(socklen_t)length);
              	if (n < 0) { 
              		perror("Sendto");
              		exit(1);
              	}
            	return 1;	
	}
}

void cliente(int te, int tp, char *pedido, char *ip) {
        char *buffer;
        Datagrama *enviar = NULL;
        int fin, erro = 0;
	FILE *f = NULL;
	ConState *con;
        sock = socket(PF_INET, SOCK_DGRAM, 0); //Abrir o socket com NONBLOCKING
        //fcntl1 = fcntl(sock, F_SETFL, O_NONBLOCK); if (fcntl1 < 0) {perror("fcntl"); exit(1);}

        if (sock<0){
                perror("socket");
                exit(1);
                }

        //server = gethostbyname("192.168.2.4");
	server = gethostbyname(ip);
        memset(&serv_addr, '\0', (size_t)sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
        serv_addr.sin_port = htons(portno);

        length = sizeof(struct sockaddr_in);
        
        con = conecta(sock);
        if(!con){
        	printf("Conexão mal sucedida\n");
        	exit(1);
        }
	enviar = trataPedido(pedido);
	buffer = muxDatagrama(enviar);
	if(buffer){
		enviaPedido(sock, buffer,&from, te, tp);
		erro = fin = 0;
		if(enviar->flags & FL_GET){
			f = fopen(enviar->data, "a+");
		}
		if(enviar->flags & FL_PUT){
			erro = transfereFich(te, tp, sock, enviar->data, con->clientBuffer, con, &from, length);
		}
		else {
			erro = recebeFicheiro(f, sock, &from, length);
			close(sock);
		}
	}
	else {
		printf("Comando nao encontrado\n");
		exit(0);
	}
   	if(!erro) printf("Ocorreu um erro\n");
               
}


void errorMsg() {
        puts("prog -te <val> -tp <val>");
}

Flag *contem(char **argv, char *token) {
        Flag *flag = NULL;
        int i;

        for(i=0; argv[i]!=NULL; i++) {
                if(strcasecmp(argv[i], token) == 0) { /* encontrei */
                        flag = (Flag *) malloc(sizeof(Flag));
                        flag->bool = 1;
                        flag->pos = i;
                        return flag;
                }
        }
        return flag;
}

int main(int args, char **argv) {
        Flag *tp = NULL;
        Flag *te = NULL;

        if((tp = contem(argv,"-tp"))==NULL) {
                errorMsg();
                exit(2);
        }

        if((te = contem(argv,"-te"))==NULL) {
                errorMsg();
                exit(3);
        }

        if(tp->bool && te->bool && args >5) {//a alterar aquando da programação da camada da aplicação
                cliente(atoi(argv[te->pos+1]),atoi(argv[tp->pos+1]), argv[5], argv[4]);
                /*Datagrama *teste = trataPedido(pedido);
                if(teste) mostraDatagrama(teste);
                free(teste);*/
        }
        return 0;
}
