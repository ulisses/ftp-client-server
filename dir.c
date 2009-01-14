# include <stdlib.h>
# include <stdio.h>

int main(){
	char linha[200];
	FILE *f, *p;
	
	f = fopen("dir.cmd", "w");
	
	p = popen("/bin/ls -f", "r");
	while(fgets(linha, 200, p)!=NULL) fprintf(f, "%s", linha);
	return 1;
}

