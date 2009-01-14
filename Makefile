# flags ######################################################
COMPILER = gcc
WALL = -Wall
OUTPUT = -o
NOTLINK = -c
# objects ######################################################
APL = apl
DIR = dir
CHECKSUM = checksum.o
DATAGRAMA = datagrama.o
SERVIDORC2 = serverc2
CLIENTEC2 = clientc2
ERRO = erro.o
OBJS = $(DIR) $(APL) $(CHECKSUM) $(DATAGRAMA) $(SERVIDORC2) $(ERRO) $(CLIENTEC2)
# headers ######################################################
CHECKSUM_H = checksum.h
DATAGRAMA_H = datagrama.h
ERRO_H = geradorErro.h
HEADERS = $(CHECKSUM_H) $(ERRO_H) $(DATAGRAMA_H) 
# code files
DIR_C = dir.c
SERVIDORC2_C = servidorc2.c
CLIENTEC2_C = clientec2.c
APL_C = apl.c
# makefile ######################################################
all:	
	make $(DIR)
	make $(APL)
	make $(SERVIDORC2)
	make $(CLIENTEC2)

$(DIR): $(DIR_C) 
	$(COMPILER) $(WALL) $(DIR_C) $(OUTPUT) $(DIR)

$(APL): $(APL_C) 
	$(COMPILER) $(WALL) $(APL_C) $(OUTPUT) $(APL)


$(CLIENTEC2): $(CLIENTEC2_C) $(CHECKSUM) $(ERRO) $(DATAGRAMA)
	$(COMPILER) $(WALL) $(CLIENTEC2_C) $(OUTPUT) $(CLIENTEC2)


$(SERVIDORC2): $(SERVIDORC2_C) $(DATAGRANA)
	$(COMPILER) $(WALL) $(SERVIDORC2_C) $(OUTPUT) $(SERVIDORC2)

$(CHECKSUM): $(CHECKSUM_H)
	$(COMPILER) $(WALL) $(NOTLINK) $(CHECKSUM_H) $(OUTPUT) $(CHECKSUM)

$(ERRO): $(ERRO_H)
	$(COMPILER) $(WALL) $(NOTLINK) $(ERRO_H) $(OUTPUT) $(ERRO)

$(DATAGRAMA): $(DATAGRAMA_H)
	$(COMPILER) $(WALL) $(NOTLINK) $(DATAGRAMA_H) $(OUTPUT) $(DATAGRAMA)

clean:
	rm *~ $(OBJS)
