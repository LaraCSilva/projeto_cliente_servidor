
all: meu_navegador meu_servidor

#Cliente
meu_navegador: meu_navegador.c
	gcc meu_navegador.c -o meu_navegador

#Servidor
meu_servidor: meu_servidor.c
	gcc meu_servidor.c -o meu_servidor

#Limpar os arquivos compilados
clean:
	rm -f meu_navegador meu_servidor