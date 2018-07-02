/* audio capture process */
#include <tinyalsa/asoundlib.h>
#include "sys/un.h"
#include "sys/socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <log/log.h>

#define LOCAL_SOCKET_NAME "/data/ktv_local_socket"
int capturing = 1;



void sigint_handler(int sig)
{
    capturing = 0;
}

int main(int argc, char **argv)
{
	
	
	
    int server_sockfd, client_sockfd;
    int server_len, client_len;
    struct sockaddr_un server_address;      /*声明一个UNIX域套接字结构*/
    struct sockaddr_un client_address;
    int i, bytes;
    char ch_recv[2048];
    int ret;
    
    
    while(1)
    {
	    /*创建 socket, 通信协议为PF_LOCAL, SCK_STREAM 数据方式*/
	    server_sockfd = socket (PF_LOCAL, SOCK_STREAM, 0);
	    if(server_sockfd <0 )
	    {
			ALOGD("cannot create listening socket\n" );
		    fprintf(stderr,"cannot create listening socket\n" );  
			continue;   	    	
	    }
	    else
	    {
		
	    	while(1)
	    	{
				sleep(2);
				 /*配置服务器信息(通信协议)*/
				server_address.sun_family = AF_LOCAL;

				 /*配置服务器信息(socket 对象)*/
				unlink(LOCAL_SOCKET_NAME);   /*删除原有server_socket对象*/
				//server_address.sun_path[0] = '\0';
				strcpy (server_address.sun_path, LOCAL_SOCKET_NAME);

				 /*配置服务器信息(服务器地址长度)*/
				  server_len = sizeof(server_address);
				 /*绑定 socket 对象*/
				ret = bind (server_sockfd, (struct sockaddr *)&server_address, server_len);
				if(ret == -1)
				{
						ALOGD(" cannot bind server socket\n" );
					fprintf(stderr,"cannot bind server socket\n" );
					close(server_sockfd);
					unlink (LOCAL_SOCKET_NAME); 
					break;
				}
				 
				 /*监听网络,队列数为10*/
				ret = listen (server_sockfd, 10);
				if(ret == -1)
				{
				   ALOGD("cannot listen the client connect request");  
				   fprintf(stderr,"cannot listen the client connect request");  
				   close(server_sockfd);   
				   unlink(LOCAL_SOCKET_NAME);   
				   break;   				    	
				}
				chmod(LOCAL_SOCKET_NAME,00777);//设置通信文件权限 
				client_len = sizeof (client_address);
				ALOGD("Server is waiting for client connect...\n");  
				fprintf(stderr,"Server is waiting for client connect...\n");
				client_sockfd = accept (server_sockfd, (struct sockaddr *)&client_address, (socklen_t*)&client_len);
				if (client_sockfd == -1) {
				ALOGD("accept failed");  
				fprintf(stderr,"accept failed \n");
				close(server_sockfd);   
				unlink(LOCAL_SOCKET_NAME);  
				break;
				}
				memset(ch_recv,0,2048);
				while(1)
				{
					 ret =read(client_sockfd,ch_recv,512);
					 if(ret == -1 || ret == 0)
					 {
						close(client_sockfd);
						close(server_sockfd);   
						unlink(LOCAL_SOCKET_NAME);  
						ALOGD("read = %d\n",ret); 
					
						break;		
					 }
					 if(ret != 512)
					 {
						fprintf(stderr," server read = %d\n",ret); 
						ALOGD(" server read = %d\n",ret); 
					 }						
					//usleep(2);
				} 
		  	}
	  	}
  	}
	fprintf(stderr," server out \n",ret);
    return 0;
}

