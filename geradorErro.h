#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX 100
#define ERRO 1
#define PERDIDO 2
#define VAZIO -1

char *buff[MAX];
int indexx[MAX], ocupados=0, nrActual=0, perdidos=0;

int myrand(int a, int b) {
        int r, ret = 0;

        srand( (unsigned int) time(NULL));

        r = rand();

        if(a<b) {
                b++;
                ret = (r % (b-a)) + a;
        }

        return ret;
}

char *altera(char *msg) {
        msg[0] += 1;

        return msg;
}

void inicializa() {
        int i;

        for(i=0; i< MAX; i++) {
                buff[i]=NULL;
        }

        for(i = 0; i < MAX; i++)
                indexx[i] = VAZIO;
}

void popula(int pErro, int pPerda) {
        int rErro, rPerda;

        inicializa();
        nrActual=0;
        ocupados=0;
        perdidos=0;

        while(ocupados!=pErro) {
                rErro = myrand(0,MAX);
                if(indexx[rErro] == VAZIO) {
                        indexx[rErro] = ERRO;
                        ocupados++;
                }
        }
        while(perdidos!=pPerda) {
                rPerda = myrand(0,MAX);
                if(indexx[rPerda] == VAZIO) {
                        indexx[rPerda] = PERDIDO;
                        perdidos++;
                }
        }
}

char *insereErro(int pErro, int pPerda, char *msg) {
        char *ret = (char *) malloc(sizeof(char)*strlen(msg));

        if((perdidos == 0 && ocupados==0) || nrActual == MAX)
                popula(pErro, pPerda);
        if(indexx[nrActual] == ERRO) { //preencher com erro
                        buff[nrActual] = altera(msg);
                        ret = strcpy(ret, buff[nrActual]);
        }
        if(indexx[nrActual] == PERDIDO) { //preencher vazio
                        buff[nrActual] = NULL;
                        ret = NULL;
        }
        if(indexx[nrActual] == VAZIO) { //preencher normal
                        buff[nrActual] = msg;
                        ret = strcpy(ret, buff[nrActual]);
        }
        nrActual++;

        return ret;
}
