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
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define USE_AESD_CHAR_DEVICE

bool server_running = false;
#ifdef USE_AESD_CHAR_DEVICE
static const char* FILE_NAME = "/dev/aesdchar";
#else
static const char* FILE_NAME = "/var/tmp/aesdsocketdata";
#endif
pthread_mutex_t mutex;

struct Node
{
    pthread_t threadId;
    int socketClient;
    bool completed;
    struct Node *next;
};

struct Node *head = NULL;

void addToList(struct Node * node)
{
    node->next = head;
    head = node;
}

void deleteNode(struct Node** head_ref, pthread_t key)
{
    struct Node *temp = *head_ref, *prev;
 
    if (temp != NULL && temp->threadId == key) {
        *head_ref = temp->next; // Changed head
        free(temp); // free old head
        return;
    }
 
    while (temp != NULL && temp->threadId != key) {
        prev = temp;
        temp = temp->next;
    }
 
    if (temp == NULL)
        return;
 
    prev->next = temp->next;
 
    free(temp); // Free memory
}

void joinCompletedThreads()
{
    struct Node *it = head;
    while(it != NULL)
    {
        if(it->completed == true)
        {
            pthread_t threadId = it->threadId;
            printf("join %d \n", it->threadId);
            void * thread_rtn = NULL;
            pthread_join(it->threadId, &thread_rtn);
            printf("joined %d \n", it->threadId);
            it = it->next;
            deleteNode(&head, threadId);
        }
        else
        {
            it = it->next;
        }
    }
}

void printList()
{
    struct Node *it = head;
    while(it != NULL)
    {
        printf("should not come here. thread id %d\n", it->threadId);
        it = it->next;
    }
}


void setAllThreadsCompleted()
{
    struct Node *it = head;
    while(it != NULL)
    {
        it->completed = true;
        pthread_cancel(it->threadId);
        it = it->next;
    }    
}

void* threadfunc(void* thread_param)
{
    printf("thread started\n");

    struct Node *node = (struct Node *)thread_param;
    FILE *fp = NULL;
    int client = node->socketClient;
    
    printf("thread started buff\n");

    static const int packet_size = 1024*sizeof(int);
    char buffPacket[packet_size];

    printf("thread running %d\n", node->threadId);

    int position = 0;
    while(!node->completed)
    {
        printf("run client %d \n", node->completed);

        static const int buff_size = 100;
        char tmp[buff_size];
        int length = recv(client, &tmp, buff_size, 0);
        if (length != 0)
        {
            memcpy(buffPacket+position, &tmp, length);
            position += length;
            if(tmp[length - 1] == '\n')
            {
                printf("data received\n");
                printf("length %d\n", position);

                fp = fopen(FILE_NAME, "ab");
                if(fp == NULL)
                {
                    //LOG
                    printf("Error. The file %s cannot be open", FILE_NAME);
                    node->completed = true;
                }

                pthread_mutex_lock(&mutex);
                size_t nr_bytes = fwrite((char *)buffPacket, 1, position, fp);
                pthread_mutex_unlock(&mutex);
                if(nr_bytes == 0)
                {
                    printf("Error. No bytes written to the file");
                    node->completed = true;
                }
                printf("bytes written %d\n", nr_bytes);

                fclose(fp);

                printf("open file\n");
                fp = fopen(FILE_NAME, "r");
                if(fp == NULL)
                {
                    printf("Error. The file %s cannot be open", FILE_NAME);
                    node->completed = true;
                }
                printf("send to client\n");
                size_t len = 0;
                ssize_t read;
                char *buff_ptr=NULL;
                while ((read = getline(&buff_ptr, &len, fp)) != -1) 
                {
                    // printf("Retrieved line of length %d:\n", read);
                    send(client, buff_ptr, read, 0);
                }
                if(buff_ptr != NULL)
                {
                    free(buff_ptr);
                }

                if(fp != NULL)
                {
                    fclose(fp);
                }

                position = 0;
            }
        }
        else
        {
            printf("client closed\n");
            node->completed = true;
        }
    }

    printf("thread %d stopped\n", client);
}

void* threadTimerfunc(void* thread_param)
{
    struct Node *node = (struct Node *)thread_param;
    const uint32_t time_to_sleep_ms = 10*1000; //10 seconds
    struct timeval tv;
    struct tm *ptm;
    char time_string[40];
    FILE *fp = NULL;

    while(!node->completed)
    {
        printf("Sleeping \n");
        usleep(1000 * time_to_sleep_ms);
        printf("After sleeping \n");

        memset(time_string, 0, sizeof(time_string));
        gettimeofday(&tv, NULL);
        ptm = localtime(&tv.tv_sec);
        strftime(time_string, sizeof(time_string), "timestamp:%a %d %b %Y %T %z\n", ptm);

        fp = fopen(FILE_NAME, "a");
        if(fp == NULL)
        {
            printf("Error. The file %s cannot be open", FILE_NAME);
            node->completed = true;
        }

        pthread_mutex_lock(&mutex);
        size_t nr_bytes = fwrite((char *)time_string, 1, sizeof(time_string), fp);
        pthread_mutex_unlock(&mutex);
        if(nr_bytes == 0)
        {
            printf("Error. No bytes written to the file");
            node->completed = true;
        }
        printf("bytes written %d\n", nr_bytes);

        if(fp != NULL)
        {
            fclose(fp);
        }
    }
}

void startTimer()
{
    struct Node *nodeTimer = (struct Node *)malloc(sizeof(struct Node));
    nodeTimer->socketClient = 0;
    nodeTimer->completed = false;
    pthread_t threadTimerId;

    if(pthread_create(&threadTimerId, NULL, threadTimerfunc, nodeTimer) == 0)
    {
        printf("timer thread id %d\n", threadTimerId);
        nodeTimer->threadId = threadTimerId;
        addToList(nodeTimer);
    }
    else
    {
        free(nodeTimer);
    }
}

void handler(int sig, siginfo_t *info, void *context)
{
    printf("handler\n");
    if((sig == SIGINT) || (sig == SIGTERM))
    {
        server_running = false;
        printf("stop server\n");
    }
}

int main(int argc, char *argv[])
{
    int listenfd = 0, connfd = 0, s = 0;
    struct addrinfo hints;
    struct addrinfo *ret_addr_info;
    server_running = true;

    head = NULL;
    
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
    pthread_mutex_init(&mutex, NULL);

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

#ifndef USE_AESD_CHAR_DEVICE
    remove(FILE_NAME);
#endif

    while(server_running)
    {
        struct sockaddr_in addr_client;
        socklen_t client_length = 0;
        int client = accept(listenfd, (struct sockaddr*)&addr_client, &client_length);
        if(client > 0)
        {
            #ifndef USE_AESD_CHAR_DEVICE
            if(head == NULL)
            {
                startTimer();
            }
            #endif

            char buff[50];
            sprintf(buff, "Accepted connection from : %d.%d.%d.%d\n",             
            (addr_client.sin_addr.s_addr&0xFF),
            ((addr_client.sin_addr.s_addr&0xFF00)>>8),
            ((addr_client.sin_addr.s_addr&0xFF0000)>>16),
            ((addr_client.sin_addr.s_addr&0xFF000000)>>24));
            printf("%s", buff);
            //syslog(LOG_DEBUG, buff);

            pthread_t threadId;

            struct Node *newElem = (struct Node *)malloc(sizeof(struct Node));
            newElem->socketClient = client;
            newElem->completed = false;
            if(pthread_create(&threadId, NULL, threadfunc, newElem) == 0)
            {
                newElem->threadId = threadId;
                printf("thread created %d\n",threadId);
                addToList(newElem);
                printf("added to list\n");
            }
            else
            {
                printf("error creating thread\n");
                free(newElem);
            }
            printf("join completed \n");
            joinCompletedThreads();
        }
        printf("server cyclic\n");
    }

    printf("stop threads\n");
    setAllThreadsCompleted();
    printf("join threads\n");
    joinCompletedThreads();
    printList();

    printf("Server stoppped\n");

    return 0;
}