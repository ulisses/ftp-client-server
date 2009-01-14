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
#include <unistd.h>

#define TAM2 256
#define PORT 9999


typedef struct flag {
        int bool;
        int pos;
}Flag;


int sock, length, fromlen, n, portno=PORT;
struct sockaddr_in serv_addr, from;
struct hostent *server;
char *buf;

void printConState(ConState *con){
	printf("Buffer do cliente: %d\n", con->clientBuffer);
	printf("Round trip time sec: %d\n", (int)con->rtt.tv_sec);
	printf("Round trip time usec: %4d\n", (int)con->rtt.tv_usec);
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
					if(receber->len > 1 && f!=NULL) fprintf(f,"%s", receber->data);
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
	if(!fin) return 0; else return 1;
}


int sendMsg(int te, int tp, int sock, char *buffer, ConState *con, int lastSeq, struct sockaddr_in *from, int *fromlen){
	int r, n, send = 0, try = 0;
	char *msg, *cpy;
	char rec[TAM2];
	Datagrama *recebido;
	struct timeval t;
	fd_set fds;	
	
	FD_ZERO(&fds); FD_SET(sock, &fds); //inicializa fds e coloca sock em fds
	t.tv_sec = con->rtt.tv_sec +1 ; t.tv_usec = con->rtt.tv_usec; //inicializa o timeval
	
	while(!send && try < 10){
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
	- Só retransmite se receber um ack duplicado (ack indica qual o inicio dos proximos bytes que om cliente fica á espera)
	ou se ocorrer uma situação de timeout
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


/*Recolhe dados acerca da maquina remota que está a tentar criar a ligação com
o servidor, tais como, tamanho do buffer rtt e retorna informção dos mesmos ou NULL
em caso de erro*/
ConState* aceitaLigacao(int s, struct sockaddr_in *client, int clientLen){
	int try, timeout = 10;
	struct timeval t;
	fd_set fds;	
	try = 0;
	
	char *buffer;
	int n, r, conected = 0;
	Datagrama *recebido, *enviar = initDatagrama();
	ConState *con = (ConState *)malloc(sizeof(struct con_estado));
	
	/*Construir um segmento com um SYN ACK para enviar ao cliente*/
	/*Coloca tambem no campo win_size o tamanho do buffer*/
	enviar->flags = (FL_SYN | FL_ACK);
	enviar->win_size = TAM2;
	buffer = muxDatagrama(enviar);
	
	n = sendto(s,buffer,strlen(buffer), 0, (struct sockaddr *)client, clientLen);
	if (n  < 0) {
		perror("send");
		free(buffer);
		free(client);
		exit(1);
		}
	/*Esperar por um ack usando o select de forma a que o recvfrom nao bloqueie
	Desta forma pode-se obter o valor do RTT uma vez que quando o select retorna 
	devido á escrita em um dos fds ele coloca no timeval o tempo restante*/
	
	
	FD_ZERO(&fds); FD_SET(s, &fds); //inicializa fds e coloca sock em fds
	t.tv_sec = timeout; t.tv_usec = 0; //inicializa o timeval
	
        r = 1;
        
	while(!conected && try < 10 ){
	
	r = select(s + 1, &fds, NULL, NULL, &t);
	if(r < 0) {
	perror("select");
	exit(0);
	}
	if(r && FD_ISSET(s, &fds)){
		n = recvfrom(s,buffer,TAM2,0, (struct sockaddr *) client, (socklen_t*) &clientLen);
		if(n < 0){ //Nao foi lido nada
			try++;
		}
		else {
			recebido = demuxDatagrama(buffer);
			if(recebido && recebido->flags & FL_ACK){
			conected = 1;
			/*three way hand shake completo*/
			/*retiro neste ultimo segmento recevido o tamanho do buffer do cliente e tambem o rtt*/
			con->clientBuffer = recebido->win_size;
			con->rtt.tv_sec = (timeout - t.tv_sec);
			con->rtt.tv_usec = t.tv_usec;
			printConState(con);
			conected = 1;
			}
			}
		}
	try++;
	}
	if(FD_ISSET(s, &fds)) FD_CLR(s, &fds);
	if(conected) return con; else{free(con); return NULL;} 
	}

void trataPedido(int te, int tp, Datagrama *novo, int newSock, ConState *con, struct sockaddr_in *from, int fromlen){
	char *buffer;
	if((novo->flags & FL_DIR) /*&& strstr(novo->data, "dir.cmd")*/) { //corresponde ao comando DIR
		int status, pid = fork();
		if(pid == 0){//executar comando ls
			execl("./dir", NULL,NULL);
			exit(1);
		}
		wait(&status);
		transfereFich( te, tp, newSock, "dir.cmd", con->clientBuffer, con, from, fromlen);
		return ;
	}
	if((novo->flags & FL_GET) && strstr(novo->data, "wait")){
		Datagrama *res = initDatagrama();
		strcpy(res->data, "fim de wait\n");
		printf("A preparar para domir...\n");
		sleep(10);
		printf("Acordou\n");
		res->flags = FL_FIN;
		buffer = muxDatagrama(res);
		n = sendto(newSock,buffer,strlen(buffer), 0, (struct sockaddr *)from, fromlen);
		return ;
	}
	if((novo->flags & FL_GET)) { 
		printf("get %s\n", novo->data);
		transfereFich(te, tp, newSock, novo->data, con->clientBuffer, con, from, fromlen);
		return ;
	}
	if((novo->flags & FL_PUT)) {
		FILE *f = NULL;
		f = fopen(novo->data, "w+");
		printf("put %s\n", novo->data);
		recebeFicheiro(f, newSock, from, fromlen);
		return ;
	}
	else {
		Datagrama *dados;
		dados = initDatagrama();
		dados->flags = (FL_ERRO | FL_FIN) ;
		buffer = muxDatagrama(dados);
		n = sendto(newSock,buffer,strlen(buffer), 0, (struct sockaddr *)from, fromlen);
		free(dados);
		free(buffer);
		return ;
	}
	
}


void servidor(int te, int tp) {
	Datagrama *novo;
	int pid, size;
	struct sockaddr_in *from;
	int fromlen, newSock;
        sock = socket(PF_INET, SOCK_DGRAM, 0);
        if (sock < 0)
                perror("Opening socket");
        length = sizeof(serv_addr);
        
        memset(&serv_addr, '\0', length);
        
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(portno);
        
        fromlen = sizeof(struct sockaddr_in);
        
        if (bind(sock,(struct sockaddr *)&serv_addr,length)<0)
                perror("binding");
        
        while(1) {
                buf = (char *)malloc(sizeof(char)*TAM2 +4);
                from = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
                
                n = recvfrom(sock,buf,TAM2 + 4,0, (struct sockaddr *)from,(socklen_t*) &fromlen);
                if (n < 0){
                        
                        perror("recvfrom");
                        }
                
                buf[n]='\0';
                
                /*Passar os dados que recebeu para um datagrama para os poder interpretar*/
                novo = demuxDatagrama(buf);
                if(!novo){
                printf("Formato de datagrama nao conhecido!!!\n");
                }
                free(buf);
                if(novo && novo->flags & FL_SYN){ //Tentativa de ligação
               		size = novo->len;
                		/*Filtra mensagens UDP que sejam destinadas a esta conexão*/
               		newSock =  socket(PF_INET, SOCK_DGRAM, 0);
                	if(newSock < 0) perror("socket");
                	
                	connect(newSock, (struct sockaddr *) from, sizeof(from));
                		
                		pid = fork(); //Cria processo para a ligação
                		if(pid == 0 && newSock){//processo que trata a ligação estabelecida
                			Datagrama *dados = NULL;
                			ConState *con;
                			char buffer[TAM2];
                			int r, pedido = 0, try;
					struct timeval t;
					fd_set fds;	
					try = 0;
                			if((con = aceitaLigacao(newSock, from, fromlen))==NULL) exit(0);
                			//Usar um select para não ficar eternamente á espera de pedidos
                			FD_ZERO(&fds); FD_SET(newSock, &fds); //inicializa fds e coloca sock em fds
					t.tv_sec = con->rtt.tv_sec; t.tv_usec = con->rtt.tv_usec; //inicializa o timeval
                			
                			while(try < 3 && !pedido){
		        			
		        			r = select(newSock + 1, &fds, NULL, NULL, &t);
			
						if(r && FD_ISSET(newSock, &fds)){
							n = recvfrom(newSock,buffer,TAM2+4,0, (struct sockaddr *)from,(socklen_t*) &fromlen);
							if(n < 0){
								perror("recvfrom");
								exit(0);
				  			}
				  			buffer[n] = '\0';
				  			dados = demuxDatagrama(buffer);
				  			if(dados) pedido = 1;
			  			}
			  			else {
			  				aumentaTimeout(&t);
			  				try++;
			  				}
		  			}
		        		if(pedido && dados){
		        			if(FD_ISSET(newSock, &fds)) FD_CLR(newSock, &fds);
						trataPedido(te, tp, dados, newSock, con, from, fromlen);
						free(dados);
						}	//envia pedido
		        		free(from);
		        		close(newSock);
		        		exit(0);
                		}
                	}
               /* n = sendto(sock,"Recebido\n",17, 0,(struct sockaddr *)&from,fromlen);
                if (n  < 0)
                        perror("sendto");*/
        }
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

        if(tp->bool && te->bool) {
                servidor(atoi(argv[te->pos+1]),atoi(argv[tp->pos+1]));
        }
        return 0;
}
