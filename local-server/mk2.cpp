#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>

int send_ftp_command (char* buf, std::string command, int sock);
int send_ftp_command (char* buf, std::string command, int sock){

    memset(buf, 0, 4000);

    char buf1[400];
    int ret = 0;
    int n = 0;

    n = strlen(command.c_str());

    char str[n+2];
    
    strcpy(str, command.c_str());
    
    str[n] = '\r';
    str[n+1] = '\n';
    
    send (sock, str, sizeof(str), 0);

    ret = recv (sock, buf1, sizeof(buf1)-1, 0);

    strcpy(buf, buf1);

    buf[ret] = '\0';
    printf ("Buf: %s\n", buf);
    return ret;
}

int main (){
    int sock, ret, sock1;
    char buf[4000];
    char adr[] = "31337.1434.ru";
    
    sock = socket (AF_INET, SOCK_STREAM, 0);
    sock1 = socket (AF_INET, SOCK_STREAM, 0);
    
    if (sock < 0 || sock1 < 0) {
        printf("Error in socket creation\n");
        return 1;
    }
      
    struct hostent *h;
    
    h = gethostbyname (adr);
    
    if (h == NULL) {
        printf("error/n");
        return 1;
    }
    
    struct sockaddr_in addr;
    
    memcpy (&(addr.sin_addr.s_addr), h -> h_addr_list[0], 4);
    addr.sin_port = htons (2121);
    addr.sin_family = AF_INET;
    
    ret = connect (sock, (struct sockaddr *)&addr, sizeof(addr));
    
    if (ret < 0) {
        printf ("Connection failed to remote host\n");
        close (sock);
        return 0;
    }
      
    printf ("Connection1 successful!!\n");

    send_ftp_command (buf, "USER anonymous", sock);
    
    send_ftp_command (buf, "TYPE I", sock);

    ret = send_ftp_command (buf, "PASV", sock);
      
    char arr1[10];
    char arr2[10];
    int i, j = 0, index = 0;
        
    for (i = 38; i < ret-4; i++){
        if (buf[i] == ','){
            index = 1;
            j = 0;
        }
        if (index == 0){
            arr1[j] = buf[i];
            j++;
        }
        if (index == 1){
            arr2[j] = buf[i+1];
            j++;
        }
    }
      
    int c, d;
    char adr1[] = "93.180.5.3";
    
    c = atoi (arr1);
    d = atoi (arr2);
      
    h = gethostbyname (adr1);
    
    if (h == NULL) {
        printf ("error\n");
        close (sock);
        return 1;
    }
    else
        printf ("success\n");
      
    struct sockaddr_in newaddr;
    
    newaddr.sin_port = htons(c*256+d);
    newaddr.sin_family = AF_INET;
    memcpy(&(newaddr.sin_addr.s_addr), h -> h_addr_list[0], 4);
    
    ret = connect (sock1, (struct sockaddr *)&newaddr, sizeof(newaddr));
    if (ret<0) {
        printf("Connection failed to remote host\n");
        close(sock1);
        close(sock);
        return 1;
    }
    printf("Connection2 successful!!\n");

    send (sock, "RETR N8wOWK.dat\r\n", 17, 0);
      
    FILE* f;
    f = fopen("a.txt", "wb");
    
    if (f == 0) {
        printf ("Can't open/create file\n");
        return 1;
    }
    
    ret = recv(sock1, buf, sizeof(buf)-1, 0);
    buf[ret] = '\0';
    
    if (ret <= 0) {
        printf ("error\n");
        close (sock);
        close (sock1);
        return 1;
    }
    
    while (ret > 0) {
        printf ("receive_BEGIN %s\n", buf);
        
        fwrite (buf, sizeof(char), ret, f);
        
        printf ("finish\n");
        
        ret = recv (sock1, buf, sizeof(buf)-1, 0);
        buf[ret] = '\0';
    }
    
    fclose(f);
    close(sock);
    close(sock1);
    
    return 0;
}
