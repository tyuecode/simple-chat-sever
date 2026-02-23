#include <sys/socket.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <arpa/inet.h> 
#include <pthread.h> 
#include <unistd.h> 
#include <sys/epoll.h> 
#include <fcntl.h> 
#include <errno.h> 

#define EVENTS_MAX 100      //epoll事件缓存大小
#define BUFFER_SIZE 1024    //读写缓存大小
#define GROUP_SIZE 100      //群组最大用户数




int main(int argc, char const *argv[])
{
    //创建读写缓存
    char *read_buf = malloc(BUFFER_SIZE*sizeof(char));
    char *write_buf = malloc(BUFFER_SIZE*sizeof(char));
    char *message = malloc(BUFFER_SIZE + 20);
    //创建地址结构体
    struct sockaddr_in server_addr,client_addr;
    //清空值
    memset(&server_addr,0,sizeof(server_addr));
    memset(&client_addr,0,sizeof(client_addr));
    //填写地址结构体
    server_addr.sin_family =AF_INET;            //选择ipv4协议
    server_addr.sin_addr.s_addr = INADDR_ANY;   //地址（服务端的任意地址）
    server_addr.sin_port = htons(6666);         //端口号

    //创建socket
    int socket_fd = socket(AF_INET,SOCK_STREAM,0);
    //绑定地址
    bind(socket_fd,(struct sockaddr *)&server_addr,sizeof(server_addr));
    //设置socket_fd为非阻塞状态
    int opts = fcntl(socket_fd,F_GETFL);
    opts |= O_NONBLOCK;
    fcntl(socket_fd,F_SETFL,opts);
    //创建epoll
    int epoll_fd = epoll_create1(0);
    //定义events结构体和实例
    struct epoll_event ev,events[EVENTS_MAX];
    //将socket_fd加入epoll
    ev.data.fd = socket_fd;
    ev.events = EPOLLIN;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,socket_fd,&ev);
    //开启监听
    listen(socket_fd,100);
    printf("--------服务端开始监听--------\n");




    //用于和客户端通信的文件描述
    int client_fd;
    socklen_t client_len;
    //记录当前在线用户文件描述符和昵称
    struct group
    {
        int group_id;
        char group_name[10];
    };
    struct group group[GROUP_SIZE];
    //当前group中共有多少用户描述符
    int group_count = 0;
    //记录每次wait触发事件的数量
    int event_num;
    while (1)
    {
        event_num = epoll_wait(epoll_fd,events,EVENTS_MAX,-1);
        for (size_t i = 0; i < event_num; i++)
        {
            if(events[i].data.fd == socket_fd)
            {
                //获取客户端文件描述符
                client_len = sizeof(client_addr);
                client_fd = accept(socket_fd,(struct sockaddr *)&client_addr,&client_len);
                //设置client_fd为非阻塞状态
                opts = fcntl(client_fd,F_GETFL);
                opts |= O_NONBLOCK;
                fcntl(client_fd,F_SETFL,opts);
                //将client_fd加入epoll
                ev.data.fd = client_fd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(epoll_fd,EPOLL_CTL_ADD,client_fd,&ev);
                printf("来自地址%s端口%d文件描述符%d成功连接\n",inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port),client_fd);
                //将新用户加入group
                group[group_count].group_id = client_fd;
                group_count++;
                sprintf(message,"你已成功连接到聊天室\n"); 
                send(client_fd,message,strlen(message),0);
            }
            else if (events[i].events & EPOLLIN)
            {
                ssize_t read_count = 0,write_count = 0;
                client_fd = events[i].data.fd;
                while ((read_count = recv(client_fd,read_buf,BUFFER_SIZE,0))>0)
                {    
                    //判断是否为私聊 格式为"/@文件描述符 消息"
                    if(strncmp(read_buf,"/@",2) == 0)
                    {
                        int target;
                        char content[1024];
                        if(sscanf(read_buf+2,"%d %[^\n]",&target,content) == 2)
                        {
                            for (size_t i = 0; i < group_count; i++)
                            {
                                if (target == group[i].group_id)
                                {
                                snprintf(message,BUFFER_SIZE+20,"[私聊]%d:%s\n",client_fd,content); 
                                send(target,message,strlen(message),0);
                                target = 0;
                                break;
                                }
                            }
                            if (target != 0)
                            {
                                sprintf(message,"你的私聊对象不存在\n"); 
                                send(client_fd,message,strlen(message),0);
                            }
                        }
                        else
                        {
                            sprintf(message,"你的私聊格式错误,正确格式为/@文件描述符 消息\n"); 
                            send(client_fd,message,strlen(message),0);
                        }
                    }
                    //改名
                    else if(strncmp(read_buf,"/name:",6) == 0)
                    {
                        for (size_t i = 0; i < group_count; i++)
                        {
                            if (client_fd == group[i].group_id)
                            {
                            sprintf(group[i].group_name,"%s",read_buf+6); 
                            snprintf(message,BUFFER_SIZE+20,"%d[%s]加入群聊\n",client_fd,group[i].group_name);
                            break;
                            }
                        }
                        for (size_t i = 0; i < group_count; i++)
                        {
                            if (group[i].group_id != client_fd)
                            {
                                send(group[i].group_id,message,strlen(message),0);
                            }                       
                        }
                    }
                    //群发
                    else
                    {   
                        //将用户名和信息打包成message 
                        for (size_t i = 0; i < group_count; i++)
                        {
                            if (client_fd == group[i].group_id)
                            {
                            snprintf(message,BUFFER_SIZE+20,"%d[%s]:%s",client_fd,group[i].group_name,read_buf);
                            break;
                            }
                        }   
                        //向group内所有用户转发信息
                        for (size_t i = 0; i < group_count; i++)
                        {
                            if (group[i].group_id != client_fd)
                            {
                                send(group[i].group_id,message,strlen(message),0);
                            }                       
                        }
                    }
                    //给read_buf加入字符串终止符，防止printf时越界
                    read_buf[read_count] = '\0';
                    printf("收到来自%d的信息:%s",client_fd,read_buf);
                    memset(read_buf,0,BUFFER_SIZE);
//                    strcpy(write_buf,"收到~\n");
//                    write_count = send(client_fd,write_buf,strlen(write_buf),0);
//                    memset(write_buf,0,BUFFER_SIZE);
                }
                if (read_count == -1 && errno == EAGAIN)
                {
                    printf("------------------------------------------------------\n");
                }
                else if (read_count == 0)
                {
                    printf("客户端%d请求退出\t",client_fd);
                    strcpy(write_buf,"服务器收到退出请求\n");
                    send(client_fd,write_buf,strlen(write_buf),0);
                    memset(write_buf,0,BUFFER_SIZE);
                    //将用户踢出group
                    for (size_t i = 0; i < group_count; i++)
                    {
                        if (group[i].group_id == client_fd)
                        {
                            //广播退出消息
                            snprintf(message,BUFFER_SIZE+20,"%d[%s]退出群聊\n",client_fd,group[i].group_name);
                            for (size_t i = 0; i < group_count; i++)
                            {
                                if (group[i].group_id != client_fd)
                                {
                                    send(group[i].group_id,message,strlen(message),0);
                                }                       
                            }
                            //清除group结构体
                            //如果是组尾用户退出
                            if (i == (group_count - 1))
                            {
                                group[i].group_id = 0;
                                memset(group[i].group_name,0,10);
                                group_count--;
                                break;
                            }
                            //清除空位
                            for(;i < group_count; i++)
                            {
                                group[i] = group[i + 1];
                            }
                            group_count--;
                            break;
                        }
                    }
                    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,client_fd,NULL);
                    shutdown(client_fd,SHUT_RDWR);
                    close(client_fd);
                    printf("成功退出\n");
                }
            }
        }
    }
    free(read_buf);
    free(write_buf);
    free(message);
    close(epoll_fd);
    close(socket_fd);
    printf("释放资源\n");

    return 0;
}
