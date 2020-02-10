#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int sock, data_sock;
char *ip_addr;

int err_handler(int func, const char *errstr)
{
    if(func < 0)
    {
        if(sock)
            close(sock);
        if(data_sock)
            close(data_sock);
        perror(errstr);
        exit(-1);
    }
    return func;
}

int init_sock()
{
    int len;
    struct sockaddr_in addr;
    int result;
    sock = err_handler(socket(AF_INET, SOCK_STREAM, 0), "socket");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(21);
    inet_pton(AF_INET, ip_addr, &addr.sin_addr.s_addr);
    len = sizeof(addr);
    result = err_handler(connect(sock, (struct sockaddr *)&addr, len), "connect");
    return sock;
}

int init_data()
{
    if(data_sock)
        close(data_sock);
    char *tmp_char;
    int i, a, d, r, p, t;
    int len, result;
    struct sockaddr_in addr;;
    char buff[256];

    send(sock,"PASV\n",strlen("PASV\n"),0);
    recv(sock, buff, 256, 0);
    printf("%s\n", buff);

    tmp_char = strtok(buff,"(");
    tmp_char = strtok(NULL,"(");
    tmp_char = strtok(tmp_char, ")");

    sscanf(tmp_char, "%d,%d,%d,%d,%d,%d",&i,&a,&d,&r,&p,&t);
    int port = (p << 8) + t;

    data_sock = err_handler(socket(AF_INET, SOCK_STREAM, 0), "socket");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_addr, &addr.sin_addr.s_addr);
    len = sizeof(addr);
    result = err_handler(connect(data_sock, (struct sockaddr *)&addr, len), "connect");
    return 0;
}

int read_response(int sock)
{
    int result;
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    do
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        char buff[2048] = {' '};
        if(recv(sock, &buff, 2048, 0) == 0)
            return 0;
        printf("%s\n", buff);
        if(strstr(buff, "530") != NULL || strstr(buff, "221") != NULL)
            return -1;
        result = select(sock + 1, &readfds, NULL, NULL, &timeout); 
    }while(result);
    return 1;
}

int send_command(char *buf, int size)
{
    send(sock, buf, size, 0);
    if(read_response(sock) == -1)
        return -1;
    return 0;
}

int login()
{
    char name[64];
    char pass[64]; 
    char str[512];

    printf("Введите имя\n");
    scanf("%s", name);
    sprintf(str,"USER %s\n", name);
    send_command(str, strlen(str));

    printf("Введите пароль\n");
    scanf("%s", pass);
    sprintf(str,"PASS %s\n", pass);
    if(send_command(str, strlen(str)) == -1)
        return -1;
    return 0;
}

int get(char *file, char *name)
{
    char str[128];
    char size[512];
    int file_size;
    char *tmp_size;
    FILE *f;
    int read = 0;

    sprintf(str, "RETR %s\n", file);
    send(sock, str, strlen(str), 0);
 
    recv(sock, size, 512, 0);
    printf("size: %s\n", size);
    if(strstr(size, "150") == NULL)
        return -1;
 
    tmp_size = strtok(size,"(");
    tmp_size = strtok(NULL,"(");
    tmp_size = strtok(tmp_size, " ");
 
    sscanf(tmp_size,"%d", &file_size);
    f = fopen(name, "wb");
    do{
        char buff[2048];
        int readed = recv(data_sock, buff, sizeof(buff), 0);
        fwrite(buff, sizeof(char), readed, f);
        read += readed;
    }while(read < file_size);

    fclose(f);
    read_response(sock);
    return 0;
}


int list()
{
    send_command("LIST\n", strlen("LIST\n"));
    read_response(data_sock);
    if(init_data() == -1)
    {
        close(sock);
        exit(-1);
    }
    return 1;
}

int main(int argc, char **argv)
{
    int command, cycle = 1;
    if(argc > 1)
        ip_addr = argv[2];
    else
        ip_addr = "127.0.0.1";
    sock = init_sock();
    read_response(sock);
    if(login() == -1)
    {
        close(sock);
        printf("Неправильный пароль\n");
        exit(-1);
    }
    init_data();
    while(cycle)
    {
        char command_str[256];
        printf("Введите команду\n");
        scanf("%s", command_str);
        command = -1;
        if(!strcmp(command_str, "quit"))
            command = 0;
        if(!strcmp(command_str, "get"))
            command = 1;
        if(!strcmp(command_str, "help"))
            command = 2;
        if(!strcmp(command_str, "user"))
            command = 3;
        if(!strcmp(command_str, "list"))
            command = 4;
        if(!strcmp(command_str, "cd"))
            command = 5;
        if(!strcmp(command_str, "chmod"))
            command = 6;
        if(!strcmp(command_str, "mkdir"))
            command = 7;
        if(!strcmp(command_str, "cdup"))
            command = 8;
        if(!strcmp(command_str, "pwd"))
            command = 9;
        if(!strcmp(command_str, "size"))
            command = 10;

        switch(command)
        {
            case 0:
                send_command("QUIT\n", strlen("QUIT\n"));
                cycle = 0;
                break;
            case 1:
            {
                char name[64];
                char name1[64];
                printf("Введите старое название файла\n");
                scanf("%s", name);
                printf("Введите новое название файла\n");
                scanf("%s", name1);
                get(name, name1);
                if(init_data() == -1)
                {
                    close(sock);
                    exit(-1);
                }
                break;
            }
            case 2:
                printf("quit\t- выход\n"
                        "get\t- скачать файл\n"
                        "help\t- помощь\n"
                        "user\t- сменить пользователя\n"
                        "list\t- вывод содержимого папки\n"
                        "cd\t- перейти в другую директорию\n"
                        "chmod\t- изменить права доступа\n"
                        "mkdir\t- создать папку\n"
                        "cdup\t- в родительскую директорию\n"
                        "pwd\t- текущая директория\n"
                        "size\t- размер файла\n"
                        );      
                break;
            case 3:
                if(login() == -1)
                {
                    close(sock);
                    close(data_sock);
                    printf("Неправильный пароль\n");
                    exit(-1);
                }
                break;
            case 4:
                list();
                break;
            case 5:
            {
                char name[64];
                char str[128];
                printf("Введите название папки\n");
                scanf("%s", name);
                sprintf(str,"CWD %s\n", name);
                send_command(str, strlen(str));
                break;
            }
            case 6:
            {
                char name[64];
                char mode[16];
                char str[128];
                printf("Введите новые правила \n");
                scanf("%s", mode);
                printf("Введите имя файла\n");
                scanf("%s", name);
                sprintf(str,"CHMOD %s %s\n", mode, name);
                send_command(str, strlen(str));
                break;
            }
            case 7:
            {
                char name[64];
                char str[128];
                printf("Введите имя папки\n");
                scanf("%s", name);
                sprintf(str,"MKD %s\n", name);
                send_command(str, strlen(str));
                break;
            }
            case 8:
                send_command("CDUP\n", strlen("CDUP\n"));
                break;
            case 9:
                send_command("PWD\n", strlen("PWD\n"));
                break;
            case 10:
            {
                char name[64];
                char str[128];
                printf("Введите имя файла\n");
                scanf("%s", name);
                sprintf(str,"SIZE %s\n", name);
                send_command(str, strlen(str));
                break;
            }

            default:
                printf("Неизвестная команда. Введите help для показа доступных команд\n");
                break;
        }
    }
    close(sock);
    exit(0);
}

