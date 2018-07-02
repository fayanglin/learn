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
    struct sockaddr_un server_address;      /*����һ��UNIX���׽��ֽṹ*/
    struct sockaddr_un client_address;
    int i, bytes;
    char ch_recv[2048];
    int ret;
    
    
    while(1)
    {
	    /*���� socket, ͨ��Э��ΪPF_LOCAL, SCK_STREAM ���ݷ�ʽ*/
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
				 /*���÷�������Ϣ(ͨ��Э��)*/
				server_address.sun_family = AF_LOCAL;

				 /*���÷�������Ϣ(socket ����)*/
				unlink(LOCAL_SOCKET_NAME);   /*ɾ��ԭ��server_socket����*/
				//server_address.sun_path[0] = '\0';
				strcpy (server_address.sun_path, LOCAL_SOCKET_NAME);

				 /*���÷�������Ϣ(��������ַ����)*/
				  server_len = sizeof(server_address);
				 /*�� socket ����*/
				ret = bind (server_sockfd, (struct sockaddr *)&server_address, server_len);
				if(ret == -1)
				{
						ALOGD(" cannot bind server socket\n" );
					fprintf(stderr,"cannot bind server socket\n" );
					close(server_sockfd);
					unlink (LOCAL_SOCKET_NAME); 
					break;
				}
				 
				 /*��������,������Ϊ10*/
				ret = listen (server_sockfd, 10);
				if(ret == -1)
				{
				   ALOGD("cannot listen the client connect request");  
				   fprintf(stderr,"cannot listen the client connect request");  
				   close(server_sockfd);   
				   unlink(LOCAL_SOCKET_NAME);   
				   break;   				    	
				}
				chmod(LOCAL_SOCKET_NAME,00777);//����ͨ���ļ�Ȩ�� 
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

