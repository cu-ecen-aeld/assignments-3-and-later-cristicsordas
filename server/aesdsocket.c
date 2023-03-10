#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <arpa/inet.h>

int server_running = 0;
static const char* FILE_NAME = "/var/tmp/aesdsocketdata";

void handler(int sig, siginfo_t *info, void *context)
{
    if((sig == SIGINT) || (sig == SIGTERM))
    {
        server_running = 1;
    }
}

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0, s = 0;
    struct addrinfo hints;
    struct addrinfo *ret_addr_info;
    FILE *fp = NULL;

    int start_daemon = 0;
    if(argc == 2)
    {
        if(strcmp(argv[1], "-d") == 0)
        {
            start_daemon = 1;
        }
    }

    openlog(NULL, 0, LOG_USER);
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
    {
        printf("cannot create socket\n");
        return -1;
    }

    s = getaddrinfo(NULL, "9000", &hints, &ret_addr_info);
    if (s != 0) 
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }    

    if(bind(listenfd, ret_addr_info->ai_addr, ret_addr_info->ai_addrlen) != 0)
    {
        printf("bind error\n");
        return -1;
    }

    freeaddrinfo(ret_addr_info);

    if(start_daemon == 1)
    {
        printf("fork \n");
        pid_t pid = fork();
        if(pid == 0)
        {
            printf("I am the child \n");
        }
        else if(pid > 0)
        {
            printf("I am the parent\n");
            exit(0);
        }
        else
        {
            printf("Error in fork\n");
            return 0;
        }
    }

    if(listen(listenfd, 100) != 0)
    {
        printf("server listen error\n");
        return -1;
    }

    struct sigaction act_int = { 0 };
    act_int.sa_sigaction = &handler;
    if (sigaction(SIGINT, &act_int, NULL) == -1) 
    {
        printf("set sigint error\n");
        return -1;
    }
    struct sigaction act_term = { 0 };
    act_term.sa_sigaction = &handler;
    if (sigaction(SIGTERM, &act_term, NULL) == -1) 
    {
        printf("set sigterm error\n");
        return -1;
    }

    remove(FILE_NAME);

    while(server_running == 0)
    {
        struct sockaddr_in addr_client;
        socklen_t client_length = 0;
        int client = accept(listenfd, (struct sockaddr_in*)&addr_client, &client_length);
        if(client > 0)
        {
            char buff[50];
            sprintf(buff, "Accepted connection from : %d.%d.%d.%d\n",             
            (addr_client.sin_addr.s_addr&0xFF),
            ((addr_client.sin_addr.s_addr&0xFF00)>>8),
            ((addr_client.sin_addr.s_addr&0xFF0000)>>16),
            ((addr_client.sin_addr.s_addr&0xFF000000)>>24));
            printf(buff);
            syslog(LOG_DEBUG, buff);
        }

        static const int packet_size = 1024*1024;
        char *buffPacket = (char*)malloc(packet_size*sizeof(int));
        if(buffPacket == 0)
        {
            printf("error allocating memory\n");
            return -1;
        }
        int position = 0;

        int client_running = 0;
        while(server_running == 0 && client_running == 0)
        {
            static const int buff_size = 100;
            char *tmp = malloc(buff_size);
            int length = recv(client, tmp, buff_size, 0);
            if (length != 0)
            {
                memcpy(buffPacket+position, tmp, length);
                position += length;
                if(tmp[length - 1] == '\n')
                {
                    printf("data received\n");
                    printf("length %d\n", position);

                    //write buffPacket to file
                    fp = fopen(FILE_NAME, "ab");
                    if(fp == NULL)
                    {
                        //LOG
                        printf("Error. The file %s cannot be open", FILE_NAME);
                        return -1;
                    }

                    size_t nr_bytes = fwrite((char *)buffPacket, 1, position, fp);
                    if(nr_bytes == 0)
                    {
                        printf("Error. No bytes written to the file");
                        return 1;
                    }
                    printf("bytes written %d\n", nr_bytes);

                    fclose(fp);

                    printf("open file\n");
                    fp = fopen(FILE_NAME, "r");
                    if(fp == NULL)
                    {
                        printf("Error. The file %s cannot be open", FILE_NAME);
                        return -1;
                    }
                    printf("send to client\n");
                    size_t len = 0;
                    ssize_t read;
                    char *buff = malloc(1000);
                    while ((read = getline(&buff, &len, fp)) != -1) 
                    {
                        printf("Retrieved line of length %d:\n", read);
                        send(client, buff, read, 0);
                    }
                    free(buff);

                    fclose(fp);
                    position = 0;
                }
            }
            else
            {
                client_running = 1;
            }
            free(tmp);
        }
        free(buffPacket);
    }

    printf("Server stoppped\n");

    return 0;
}