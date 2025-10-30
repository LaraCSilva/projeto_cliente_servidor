#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>    
#include <dirent.h>

#define PORTA 5050
#define TAMANHO_BUFFER 16384
#define MAX_CAMINHO 1024

char* diretorio_raiz;

//Envia uma resposta de erro para o cliente
void envia_erro404(int socket_cliente){
    const char* resposta = "HTTP/1.0 404 Not Found\r\n"
                           "Content-Type: text/html\r\n"
                           "Connection: close\r\n\r\n"
                           "<html><head><title>404 Not Found</title></head>"
                           "<body>"
                           "<h1>404 Not Found</h1>"
                           "<p>O recurso solicitado nao foi encontrado no servidor.</p>"
                           "</body></html>";
    send(socket_cliente, resposta, strlen(resposta), 0);
}

//Realiza o socket do servidor
int inicia_servidor(int porta){
    int socket_servidor;
    struct sockaddr_in endereco_servidor;

    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_servidor < 0){
        printf("Não foi possível abrir o socket\n");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&endereco_servidor, 0, sizeof(endereco_servidor));
    endereco_servidor.sin_family = AF_INET;
    endereco_servidor.sin_addr.s_addr = INADDR_ANY;
    endereco_servidor.sin_port = htons(porta);

    if(bind(socket_servidor, (struct sockaddr*)&endereco_servidor, sizeof(endereco_servidor)) < 0){
        printf("Falha no bind\n");
        exit(EXIT_FAILURE);
    }

    if(listen(socket_servidor, 5) < 0){
        printf("Falha na escuta\n");
        close(socket_servidor);
        exit(EXIT_FAILURE);
    }

    return socket_servidor;
}

// Retorna o tipo do MIME  com base na extensão do arquivo
const char* obter_tipo_mime(const char* nome_arquivo) {
    const char* extensao = strrchr(nome_arquivo, '.');
    if (!extensao) {
        return "application/octet-stream";
    }

    if (strcmp(extensao, ".html") == 0 || strcmp(extensao, ".htm") == 0) {
        return "text/html";
    }
    if (strcmp(extensao, ".txt") == 0) {
        return "text/plain";
    }
    if (strcmp(extensao, ".css") == 0) {
        return "text/css";
    }
    if (strcmp(extensao, ".js") == 0) {
        return "application/javascript";
    }
    if (strcmp(extensao, ".jpg") == 0 || strcmp(extensao, ".jpeg") == 0) {
        return "image/jpeg";
    }
    if (strcmp(extensao, ".png") == 0) {
        return "image/png";
    }
    if (strcmp(extensao, ".gif") == 0) {
        return "image/gif";
    }
    if (strcmp(extensao, ".pdf") == 0) {
        return "application/pdf";
    }

    return "application/octet-stream"; 
}

//Envia o arquivo para o cliente
void envia_arquivo(int socket_cliente, const char* caminho_arquivo){
    FILE* arquivo = fopen(caminho_arquivo, "rb");

    if(arquivo == NULL){
        envia_erro404(socket_cliente);
        return;
    }

    //Acha o tamanho do arquivo
    fseek(arquivo, 0, SEEK_END);
    long int tamanho_arquivo = ftell(arquivo);
    fseek(arquivo, 0, SEEK_SET);

    const char* tipo_mime = obter_tipo_mime(caminho_arquivo);

    //Envia o cabeçalho do arquivo
    char cabecalho[TAMANHO_BUFFER];
    sprintf(cabecalho, "HTTP/1.0 200 OK\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %ld\r\n"
                       "Connection: close\r\n\r\n", tipo_mime, tamanho_arquivo);
    send(socket_cliente, cabecalho, strlen(cabecalho), 0);

    //Envia o corpo do arquivo
    char buffer[TAMANHO_BUFFER];
    size_t bytes_lidos;
    while((bytes_lidos = fread(buffer, 1, TAMANHO_BUFFER, arquivo)) > 0){
        if(send(socket_cliente, buffer, bytes_lidos, 0) < 0){
            break;
        }
    }
    fclose(arquivo);
}

//Caso não tenha o index.html ele manda a listagem de arquivos existentes no diretório
void envia_listagem_arquivos(int socket_cliente, const char* caminho_diretorio, const char* caminho_req){
    DIR* dir = opendir(caminho_diretorio);

    if(dir == NULL){
        envia_erro404(socket_cliente);
        return;
    }

    char buffer[TAMANHO_BUFFER];
    sprintf(buffer, "HTTP/1.0 200 OK\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                    "Connection: close\r\n\r\n");
    if(send(socket_cliente, buffer, strlen(buffer), 0) < 0){
        closedir(dir);
        return;
    }

    sprintf(buffer, "<html><head><title>Lista de arquivos disponíveis</title></head>"
                    "<body><h1>Lista de Arquivos Disponíveis</h1><ul>\n");
    if(send(socket_cliente, buffer, strlen(buffer), 0) < 0){
        closedir(dir);
        return;
    }

    struct dirent* entrada_diretorio;
    while((entrada_diretorio = readdir(dir)) != NULL){
        if(strcmp(entrada_diretorio->d_name, ".") == 0 || strcmp(entrada_diretorio->d_name, "..") == 0){
            continue;
        }

        char item_lista[MAX_CAMINHO];
        sprintf(item_lista, "%s/%s", caminho_diretorio, entrada_diretorio->d_name);

        struct stat info_item;
        
        if(stat(item_lista, &info_item) == 0 && S_ISREG(info_item.st_mode)){
            
            char href_path[MAX_CAMINHO];
            char* item = entrada_diretorio->d_name;

            // Constrói o caminho para o link (href)
            if (caminho_req[strlen(caminho_req) - 1] == '/') {
                sprintf(href_path, "%s%s", caminho_req, item);
            } else {
                sprintf(href_path, "%s/%s", caminho_req, item);
            }
            
            // Envia o item da lista como um link HTML
            sprintf(buffer, "<li><a href=\"%s\">%s</a></li>\n", href_path, item);
            if(send(socket_cliente, buffer, strlen(buffer), 0) < 0){
                break; // Sai do loop se o send falhar
            }
        }
    }
    sprintf(buffer, "</ul></body></html>\n");
    send(socket_cliente, buffer, strlen(buffer), 0);

    closedir(dir);
}

//Trata a conexão do cliente
void trata_conexao(int socket_cliente){
    char buffer[TAMANHO_BUFFER];
    char metodo[20];
    char caminho_req[MAX_CAMINHO];
    char protocolo[20];
    char caminho_completo[MAX_CAMINHO];

    memset(buffer, 0, TAMANHO_BUFFER);

    int bytes_recebidos = recv(socket_cliente, buffer, TAMANHO_BUFFER - 1, 0);
    if(bytes_recebidos <= 0){
        close(socket_cliente);
        return;
    }

    if(sscanf(buffer, "%s %s %s", metodo, caminho_req, protocolo) != 3){
        printf("Requisição mal formatada\n");
        close(socket_cliente);
        return;
    }
    if(strcmp(metodo, "GET") != 0){
        printf("Método %s não suportado\n", metodo);
        close(socket_cliente);
        return;
    }
    if(strstr(caminho_req, "..")){
        envia_erro404(socket_cliente);
        close(socket_cliente);
        return;
    }

    sprintf(caminho_completo, "%s%s", diretorio_raiz, caminho_req);
    printf("Requisição: %s\nCaminho completo: %s\n", caminho_req, caminho_completo);

    struct stat info_caminho;
    if(stat(caminho_completo, &info_caminho) != 0){
        envia_erro404(socket_cliente);
    }else if(S_ISREG(info_caminho.st_mode)){
        printf("Servindo arquivo...\n");
        envia_arquivo(socket_cliente, caminho_completo);
    }else if(S_ISDIR(info_caminho.st_mode)) {
        char caminho_index[MAX_CAMINHO];

        if(caminho_completo[strlen(caminho_completo) - 1] == '/'){
            sprintf(caminho_index, "%sindex.html", caminho_completo);
        } else {
            sprintf(caminho_index, "%s/index.html", caminho_completo);
        }

        if (access(caminho_index, F_OK) == 0) {
            envia_arquivo(socket_cliente, caminho_index);
        } else {
            printf("Servindo listagem do diretório...\n");
            envia_listagem_arquivos(socket_cliente, caminho_completo, caminho_req);
        }
    }
    else{
        envia_erro404(socket_cliente);
    }

    close(socket_cliente);
    printf("Conexão fechada\n");
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Erro na digitação\n");
        printf("Certifique que está no formato ./meu_servidor /<diretório raiz>\n");
        exit(EXIT_FAILURE);
    }

    diretorio_raiz = argv[1];

    int socket_servidor = inicia_servidor(PORTA);
    int socket_cliente;
    struct sockaddr_in endereco_cliente;
    socklen_t tamanho_cliente_addr = sizeof(endereco_cliente);

    printf("Servidor ligado!!\n");

    while(1){
        socket_cliente = accept(socket_servidor, (struct sockaddr*)&endereco_cliente, &tamanho_cliente_addr);

        if(socket_cliente < 0){
            printf("Falha no accept\n");
            continue;
        }

        trata_conexao(socket_cliente);
        printf("Conexão com o cliente finalizada\n");
    }

    close(socket_servidor);
    return 0;
}

