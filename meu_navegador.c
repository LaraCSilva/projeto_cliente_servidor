#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <netdb.h>       
#include <arpa/inet.h>

#define TAMANHO_BUFFER 16384

//Extrai o nome do arquivo no final do caminho
const char* extrai_nome_arquivo(const char* caminho){
    const char* ultima_barra = strrchr(caminho, '/');
    if(ultima_barra && ultima_barra[1] != '\0'){
        return ultima_barra + 1;
    }
    if(strcmp(caminho, "/") == 0){
        return "index.html";
    }
    return "index.html";
}

//Extrai as informações da url separando o host, a porta e o caminho
void analisa_url(const char* url, char* host, int* porta, char* caminho){
    char copia_url[TAMANHO_BUFFER];
    strcpy(copia_url, url);
    
    //Remove o prefixo http://
    char* inicio_url = strstr(copia_url, "://");
    if(inicio_url){
        inicio_url += 3;
    }else{
        inicio_url = copia_url;
    }

    char* barra = strchr(inicio_url, '/');
    char* dois_pontos = strchr(inicio_url, ':');

    //Encontra o caminho
    if(barra){
        strcpy(caminho, barra);
        *barra = '\0';
    }else{
        strcpy(caminho, "/");
    }

    //Encontra a porta
    if(dois_pontos && (barra == NULL || dois_pontos < barra)){
        *porta = atoi(dois_pontos + 1);
        *dois_pontos = '\0';
    }else{
        *porta = 80;
    }

    //Encontra o host
    strcpy(host, inicio_url);
}

//Converte o nome do host em IP
struct hostent* converte_host(const char* host){
    struct hostent* servidor = gethostbyname(host);

    if(servidor == NULL){
        printf("Host %s não encontrado.\n", host);
        exit(EXIT_FAILURE);
    }
    return servidor;
}

//Cria um socket e se conecta com o servidor
int conecta_servidor(const struct hostent* servidor, int porta){
    struct sockaddr_in endereco_servidor;
    int id_socket;

    id_socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if(id_socket < 0){
        printf("Não foi possível abrir o socket\n");
        exit(EXIT_FAILURE);
    }

    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_port = htons(porta);
    memcpy(&endereco_servidor.sin_addr.s_addr, servidor->h_addr_list[0], servidor->h_length);

    if(connect(id_socket, (struct sockaddr*)&endereco_servidor, sizeof(endereco_servidor)) < 0){
        printf("Não foi possível conectar\n");
        close(id_socket);
        exit(EXIT_FAILURE);
    }
    return id_socket;
}

//Envia a requisição GET para o servidor
void envia_requisicao(int id_socket, const char* caminho, const char* host){
    char requisicao[TAMANHO_BUFFER];

    sprintf(requisicao, "GET %s HTTP/1.0\r\n"
                        "Host: %s\r\n"
                        "User_Agent: meu_navegador\r\n"
                        "Connection: close\r\n\r\n", caminho, host);

    if(send(id_socket, requisicao, strlen(requisicao), 0) < 0){
        printf("Falha ao enviar requisição\n");
        close(id_socket);
        exit(EXIT_FAILURE);
    }
}

//Recebe a resposta e salva o arquivo
int recebe_salva(int id_socket, const char* nome_arquivo){
    char buffer[TAMANHO_BUFFER];
    int bytes_recebidos;
    FILE *arquivo_saida = NULL;

    arquivo_saida = fopen(nome_arquivo, "wb");
    if(arquivo_saida == NULL){
        printf("Erro ao criar o arquivo\n");
        close(id_socket);
        exit(EXIT_FAILURE);
    }

    int cabecalho_encontrado = 0; //0 = falso, 1 = verdadeiro
    int status_checado = 0; //0 = falso, 1 = verdadeiro
    int erro_http = 0; //0 = sucesso, 1 = falha
    char *fim_cabecalho;

    //Loop principal
    while((bytes_recebidos = recv(id_socket, buffer, TAMANHO_BUFFER - 1, 0)) > 0){
        buffer[bytes_recebidos] = '\0';

        if(!cabecalho_encontrado){
            if(!status_checado){
                if(strstr(buffer, "200 OK") != NULL){
                    status_checado = 1; 
                }else {
                    if(strstr(buffer, "404 Not Found") != NULL){
                        printf("Erro: 404 Not Found - Arquivo não encontrado no servidor\n");
                    }else if(strstr(buffer, "301") != NULL || strstr(buffer, "302") != NULL){
                        printf("Erro: Não suporta redirecionamentos\n");
                    }else{
                        printf("Erro: Servidor não está OK\n");
                    }
                    
                    erro_http = 1;
                    break;
                }
            }


            fim_cabecalho = strstr(buffer, "\r\n\r\n");
    
            if(fim_cabecalho){
                cabecalho_encontrado = 1;
    
                char* inicio_corpo = fim_cabecalho + 4;
                int tamanho_corpo = bytes_recebidos - (inicio_corpo - buffer);
    
                if(tamanho_corpo > 0){
                    fwrite(inicio_corpo, 1, tamanho_corpo, arquivo_saida);
                }
            }

        }else{
            fwrite(buffer, 1, bytes_recebidos, arquivo_saida);
        }
    }

    if(bytes_recebidos < 0){
        printf("Falha ao receber dados do socket\n");
        erro_http = 1;
    }

    fclose(arquivo_saida);
    close(id_socket);
    
    return erro_http;
}


int main(int argc, char* argv[]){
    //Impede que o usuário digite comandos errados
    if(argc != 2){
        printf("Erro na digitação!\n");
        printf("Certifique que está no formato ./meu_navegador <URL completa>\n");
        exit(EXIT_FAILURE);
    }

    char* url = argv[1];
    char host[100];
    char caminho[100];
    int porta;

    analisa_url(url, host, &porta, caminho);
    const char* nome_arquivo = extrai_nome_arquivo(caminho);

    printf("Conectando...\n");
    printf("Host: %s, Porta: %d, Caminho: %s\n", host, porta, caminho);
    printf("Salvando em: %s\n", nome_arquivo);

    struct hostent* servidor = converte_host(host);

    int id_socket = conecta_servidor(servidor, porta);

    envia_requisicao(id_socket, caminho, host);

    int erro_http = recebe_salva(id_socket, nome_arquivo);

    if(erro_http == 1){
        remove(nome_arquivo);
        printf("Download falhou\n");
        exit(EXIT_FAILURE);
    }

    printf("Arquivo %s salvo com sucesso\n", nome_arquivo);
    return 0;
}