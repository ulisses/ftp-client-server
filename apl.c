#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

void trataPedido(char *cmd, char *ip){
	int pid, status;
	if((pid = fork())==0){
		int res;
		res = execl("./clientc2", "-te", "0", "-tp", "0", ip, cmd, NULL);
		if(res < 0){
			perror("execl");
		}
		exit(0);
	}
	wait(&status);
}

int main(int argc, char *argv[]){
	if(argc ==1){
		printf("erro: ./apl endereco_ip_servidor\n");
		return 0;
	}
	else {
		int sair = 0;
		char *ip = argv[1];
		char cmd[50];
		while(!sair){
			printf(">>");
			fgets(cmd, 50, stdin);
			if(strstr(cmd, "sair")) sair = 1;
			else trataPedido(cmd, ip);
		}
	return 1;
		
	}
}
