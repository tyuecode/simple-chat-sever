#include <sys/socket.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <pthread.h> 

char name[10];

void *read_from_server(void *argv)
{ 
    int sockfd = *(int *)argv; 
    char *read_buf = NULL; 
    ssize_t count = 0; 
 
    read_buf = malloc(sizeof(char) * 1024); 
    memset(read_buf,0,1024);
 
    while ((count = recv(sockfd, read_buf, 1024, 0))>0) 
    {       
        fputs(read_buf, stdout); 
        memset(read_buf,0,1024);
        printf("--------------------------------------------\n");
    } 
    if (count < 0) 
    { 
        perror("recv"); 
    } 
    printf("收到服务端的终止信号......\n"); 
    free(read_buf); 
 
    return NULL; 
} 
 
void *write_to_server(void *argv) 
{ 
    int sockfd = *(int *)argv; 
    char *write_buf = NULL; 
    ssize_t send_count; 
 
    write_buf = malloc(sizeof(char) * 1024); 
    memset(write_buf,0,1024);
 
    sprintf(write_buf,"/name:%s",name);
    send(sockfd, write_buf, strlen(write_buf), 0); 

    memset(write_buf,0,1024);
    while (fgets(write_buf, 1024, stdin) != NULL) 
    {
        if(strcmp(write_buf,"\n") != 0)
        {
        send(sockfd, write_buf, strlen(write_buf), 0); 
        memset(write_buf,0,1024);
        printf("--------------------------------------------\n");
        }
    } 
 
    printf("接收到命令行的终止信号，不再写入，关闭连接......\n"); 
    shutdown(sockfd, SHUT_WR); 
    free(write_buf); 
 
    return NULL; 
} 
 
int main(int argc, char const *argv[]) 
{ 
    printf("输入昵称：");
    scanf("%s",name);

    int sockfd; 
    pthread_t pid_read, pid_write; 
 
    struct sockaddr_in server_addr; 
 
    memset(&server_addr, 0, sizeof(server_addr)); 
 
    server_addr.sin_family = AF_INET; 
    // 连接本机 127.0.0.1 
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
    // 连接端口 6666 
    server_addr.sin_port = htons(6666); 
 
    // 创建socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);  
    // 连接server 
    connect(sockfd, (struct sockaddr *)&server_addr,sizeof(server_addr)); 
 
    // 启动一个子线程，用来读取服务端数据，并打印到 stdout 
    pthread_create(&pid_read, NULL, read_from_server, (void *)&sockfd); 
    // 启动一个子线程，用来从命令行读取数据并发送到服务端 
    pthread_create(&pid_write, NULL, write_to_server, (void *)&sockfd);
      // 主线程等待子线程退出 
    pthread_join(pid_read, NULL); 
    pthread_join(pid_write, NULL); 
 
    printf("关闭资源\n"); 
    close(sockfd); 
 
    return 0; 
}
