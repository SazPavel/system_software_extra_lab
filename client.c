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
        perror(errstr);
        exit(-1);
    }
    return func;
}

int init_sock()
{
    int len;
    struct sockaddr_in address;
    int result;
    sock = err_handler(socket(AF_INET, SOCK_STREAM, 0), "socket");
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_addr);
    address.sin_port = htons(21);
    len = sizeof(address);
    result = err_handler(connect(sock, (struct sockaddr *)&address, len), "connect");
    return sock;
}

int init_data()
{
    if(data_sock)
        close(data_sock);
    char *tmp_char;
    int i, a, d, r, p, t;
    int len, result;
    struct sockaddr_in address;;
    char buff[128];

    send(sock,"PASV\n",strlen("PASV\n"),0);
    recv(sock, buff, 128, 0);
    printf("%s\n", buff);

    tmp_char = strtok(buff,"(");
    tmp_char = strtok(NULL,"(");
    tmp_char = strtok(tmp_char, ")");

    sscanf(tmp_char, "%d,%d,%d,%d,%d,%d",&i,&a,&d,&r,&p,&t);
    int port = p * 256 + t;

    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(data_sock == -1)
    {
        perror("socket");
        close(sock);
        return -1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_addr);
    address.sin_port = htons(port);
    len = sizeof(address);
    result = connect(data_sock, (struct sockaddr *)&address, len);
    if (result == -1) {
        perror("connect");
        close(sock);
        close(data_sock);
        return -1;
    }
    return 0;
}

int readServ(int sock)
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
    if(readServ(sock) == -1)
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

    scanf("%s", pass);
    sprintf(str,"PASS %s\n", pass);
    if(send_command(str, strlen(str)) == -1)
        return -1;
    return 0;
}

int get(char *file, char *name)
{
    char str[512];
    char size[512];
    int file_size;
    char *tmp_size;
    FILE *f;
    int read = 0;

    sprintf(str, "RETR %s\n", file);
    send(sock, str, strlen(str), 0);
 
    recv(sock, size, 512, 0);
    printf("size: %s\n", size);
    if(strstr(size, "550") != NULL)
        return -1;
 
    tmp_size = strtok(size,"(");
    tmp_size = strtok(NULL,"(");
    tmp_size = strtok(tmp_size, " ");
 
    sscanf(tmp_size,"%d", &file_size);
    f = fopen(name, "wb");
    do{
        char buff[2048];
        int readed = recv(data_sock,buff,sizeof(buff),0);
        fwrite(buff,1,readed,f);
        read += readed;
    }while(read < file_size);
    fclose(f);

    readServ(sock);
    return 0;
}


int list()
{
    send_command("LIST\n", strlen("LIST\n"));
    readServ(data_sock);
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
    if(sock == -1)
        exit(-1);
    readServ(sock);
    if(login() == -1)
    {
        close(sock);
        printf("Неправильный пароль\n");
        exit(-1);
    }
    if(init_data() == -1)
        exit(-1);
    while(cycle)
    {
        printf("Введите команду\n0 - quit\n1 - get\n2 - help\n3 - user\n4 - list\n");
        scanf("%d", &command);
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
                char str[512];
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
                send_command("HELP\n", strlen("HELP\n"));
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

        }
    }
    close(sock);
    exit(0);
}

