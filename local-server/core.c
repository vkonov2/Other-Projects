#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <math.h>
#include <ctype.h>



int send_ftp_command (char buf[], char *command, int sock);
int filesize (FILE *f);
int enter_with_mainsock (char buf[]);
int enter_pasv_mode (char buf[], int sock);
int get_list (char buf[], int sock, int newsock, FILE* f_list);
int find_str (char buf[], FILE* f_list);
int find_folder (char buf[], FILE* f_list);
int open_folder (char buf[], FILE* f_list, int sock);
int stor_file (char buf[], FILE* f_code, int sock, int newsock);
int retr_file(char buf[],FILE* f_check, int sock, int newsock);



int send_ftp_command (char buf[], char *command, int sock){

    memset(buf, 0, 4000);

    char buf1[4000];
    int ret = 0;
    int i, n = 0;

    n = strlen(command);

    char str[n+5];

    strcpy(str, "tA9");
    strcat(str, command);

    printf ("user>>%s", str);
    
    str[n+3] = '\r';
    str[n+4] = '\n';
    
    send (sock, str, sizeof(str), 0);

    ret = recv (sock, buf1, 3999, 0);
    if (ret <= 0){
        printf("recv error\n");
        return -1;
    }

    strcpy(buf, buf1);
    buf[ret] = '\0';

    printf ("\nserver>>%s\n", buf);
    
    return ret;
}

int filesize (FILE *f)
  {
    int save_pos, size_of_file;
 
    save_pos = ftell (f);
    fseek (f, 0L, SEEK_END);
    size_of_file = ftell (f);
    fseek (f, save_pos, SEEK_SET);
    return (size_of_file);
  }

int enter_with_mainsock (char buf[]){
    int sock, ret;
    char adr[] = "31337.1434.ru";
    struct sockaddr_in addr;
    struct hostent *h;
      
    h = gethostbyname (adr);
    if (h == NULL) {
        printf("address error\n");
        return -1;
    }
    
    memcpy (&(addr.sin_addr.s_addr), h -> h_addr_list[0], 4);
    addr.sin_port = htons (2122);
    addr.sin_family = AF_INET;

    sock = socket (AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("error in socket creation\n");
        return -2;
    }
    
    ret = connect (sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        printf ("main connection failed to remote host\n");
        close (sock);
        return -3;
    }

    ret = recv (sock, buf, 3999, 0);
    if (ret < 0) {
        printf ("recv error\n");
        close (sock);
        return -4;
    }

    buf[ret] = '\0';

    printf("%s\n", buf);
      
    printf ("main connection successful\n\n");

    return sock;
}

int enter_pasv_mode(char buf[], int sock){
    int newsock, ret, i, j = 0, index = 0;
    char *pasv="PASV";
    char adr[] = "93.180.5.3";
    struct hostent *h;
    struct sockaddr_in addr;
    char arr1[10];
    char arr2[10];
    
    ret = send_ftp_command (buf, pasv, sock);
        
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
      
    h = gethostbyname (adr);
    
    if (h == NULL) {
        printf ("address error\n");
        close (sock);
        return -1;
    }
    
    memcpy(&(addr.sin_addr.s_addr), h -> h_addr_list[0], 4);
    addr.sin_port = htons((atoi(arr1))*256+(atoi(arr2)));
    addr.sin_family = AF_INET;

    newsock = socket (AF_INET, SOCK_STREAM, 0);
    if (newsock < 0) {
        printf("error in socket creation\n");
        return -2;
    }

    ret = connect (newsock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        printf("connection failed to remote host\n");
        close(newsock);
        close(sock);
        return -3;
    }
    printf("pasv connection successful\n\n");
    return newsock;
}

int get_list(char buf[], int sock, int newsock, FILE* f_list){
    char *list="LIST";
    int ret;

    send_ftp_command (buf, list, sock);
    
    ret = recv(newsock, buf, 3999, 0);
    if (ret <= 0) {
        printf ("recv1 error\n");
        return -1;
    }

    buf[ret] = '\0';
    
    while (ret > 0) {
        //printf ("receive_BEGIN\n%s\n", buf);
        fwrite (buf, sizeof(char), ret, f_list);
        //printf ("FINISH_receive\n");
        
        ret = recv (newsock, buf, 3999, 0);
          
        buf[ret] = '\0';
    }
    fclose(f_list);

    ret = recv(sock, buf , 3999, 0);
    if (ret <= 0){
        printf("recv3 error\n");
        return -3;
    }

    close(newsock);
    return 1;
}

int find_str (char buf[], FILE* f_list){
    int i = 0, sym;
    char *str;

    str = (char*)malloc(100*sizeof(char));
    if (!str){
        printf ("allocation error\n");
        return -1;
    }
    
    while (1) {
        sym = fgetc(f_list);
        
        if (feof(f_list) && i==0){
            free(str);
            return 0;
        } 
        else if (feof(f_list) || isspace(sym) || ispunct(sym)){
            if (i!=0) 
                break;                    
            else continue;
        } 
        else
            str[i++] = sym;
    }
    
    str[100] = '\0';
    
    strcpy(buf, str);

    free(str);
    return 1;
}

int find_folder (char buf[], FILE* f_list){
    char part_fold_name[] = "tE2YRf";

    memset(buf, 0, 4000);
          
    while (find_str(buf, f_list) == 1){
        if (strstr(buf, part_fold_name) != NULL){
            printf("folder name: %s\n\n", buf);
            break; 
        }
    }

    fclose(f_list);
    return 1;
}

int open_folder (char buf[], FILE* f_list, int sock){
    int err;
    char *cwd;

    find_folder(buf, f_list);

    cwd=(char*)malloc((3+strlen(buf))*sizeof(char));
    if (!cwd){
        printf ("allocation error\n");
        return -1;
    }

    strcpy(cwd, "CWD ");
    strcat(cwd, buf);

    err = send_ftp_command (buf, cwd, sock);
    if (err < 0){
        close(sock);
        return -2;
    }

    return 1;
}

int stor_file (char buf[], FILE* f_code, int sock, int newsock){
    int n, ret;
    char *stor="STOR core.c";

    n = filesize(f_code);

    char mk[n];

    fread (mk, sizeof(char), n, f_code);

    send_ftp_command (buf, stor, sock);

    send (newsock, mk, n, 0);

    close (newsock);

    memset(buf, 0, 4000);

    ret = recv(sock, buf , 3999, 0);
    if (ret <= 0){
        printf("recv error\n");
        close(sock);
        return -1;
    }

    buf[ret] = '\0';
    printf("%s\n", buf);

    return 1;
}

int retr_file(char buf[],FILE* f_check, int sock, int newsock){
    int ret;
    char *retr="RETR core.c";

    send_ftp_command (buf, retr, sock);
    
    ret = recv(newsock, buf, 3999, 0);
    if (ret <= 0) {
        printf ("recv error\n");
        close (newsock);
        close (sock);
        return -1;
    }

    buf[ret] = '\0';
    
    while (ret > 0) {
        //printf ("receive_BEGIN %s\n", buf);
        fwrite (buf, sizeof(char), ret, f_check);
        //printf ("finish\n");
        
        ret = recv (newsock, buf, 3999, 0);
        buf[ret] = '\0';
    }
    fclose(f_check);

    ret = recv(sock, buf , 3999, 0);
    if (ret <= 0){
        printf("recv3 error\n");
        return -2;
    }

    close(newsock);

    return 1;
}



int main (){
    int sock, newsock, err, i;
    char buf[4000];
    FILE* f_list;
    FILE* f_code;
    FILE* f_check;

    char *user="USER Konov_MA";
    char *pass="PASS uOB89OWA7T";
    char *type="TYPE I";
    char *quit="QUIT";

    memset (buf, 0, 4000);

    sock = enter_with_mainsock (buf);

    err = send_ftp_command (buf, user, sock);
    if (err < 0){
        close(sock);
        return -1;
    }

    printf("waiting 1sec\n");
    sleep(1);

    err = send_ftp_command (buf, pass, sock);
    if (err < 0){
        close(sock);
        return -2;
    }

    printf("waiting 1sec\n");
    sleep(1);

    err = send_ftp_command (buf, type, sock);
    if (err < 0){
        close(sock);
        return -3;
    }

    printf("waiting 1sec\n");
    sleep(1);

    newsock = enter_pasv_mode(buf, sock);
    if (newsock < 0){
        close(sock);
        return -4;
    }

    f_list = fopen("list.txt", "wb");
    if (f_list == 0) {
        printf ("can't open/create file\n");
        return -5;
    }

    printf("waiting 1sec\n");
    sleep(1);
    
    err = get_list (buf, sock, newsock, f_list);
    if (err < 0){
        close(sock);
        return -6;
    }

    f_list = fopen("list.txt", "r");
    if (f_list == 0) {
        printf ("can't open file\n");
        return -7;
    }

    printf("waiting 1sec\n");
    sleep(1);

    err = open_folder(buf, f_list, sock);
    if (err < 0){
        close(sock);
        return -8;
    }

    printf("waiting 1sec\n");
    sleep(1);

    newsock = enter_pasv_mode(buf, sock);
    if (newsock < 0){
        close(sock);
        return -9;
    }

    f_code = fopen("core.c", "r");
    if (f_code == 0) {
        printf ("can't open file\n");
        return -10;
    }

    printf("waiting 1sec\n");
    sleep(1);

    err = stor_file(buf, f_code, sock, newsock);
    if (err < 0){
        close(sock);
        return -11;
    }

    printf("waiting 1sec\n");
    sleep(1);

    newsock = enter_pasv_mode(buf, sock);
    if (newsock < 0){
        close(sock);
        return -12;
    }

    f_check = fopen("check.с", "wb");
    if (f_check == 0) {
        printf ("can't open file\n");
        return -13;
    }

    printf("waiting 1sec\n");
    sleep(1);

    err = retr_file(buf, f_check, sock, newsock);
    if (err < 0){
        close(sock);
        return -14;
    }

    err = send_ftp_command (buf, quit, sock);
    if (err < 0){
        close(sock);
        return -15;
    }
    
    close(sock);
    
    return 0;
}