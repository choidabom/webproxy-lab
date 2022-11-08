#include <stdio.h>
#include "csapp.h"

/*
client <=> proxy <=> server 
1. client의 clientfd => proxy의 connfd (읽어서 적어주는 과정을 해야겠지 ~)
2. proxy에서 HTTP 형식에 맞게 왔는지 valid check (일단 이건 연결을 다 하고 진행)
3. proxy의 clientfd => server의 connfd (읽어서 적어주는 과정을 해야겠지 ~)

4. server의 connfd => proxy의 clientfd
5. proxy의 clientfd => clientd의 clientfd 
*/

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* Function prototyps => 함수 순서 상관이 없게됨 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int parse_uri(char *uri_ctop, char *uri_ptos, char *host, char *port);
int parse_requesthdrs();
void read_requesthdrs(rio_t *rp);
void doit(int fd); 
void do_client(int connfd, int clientfd);

int main(int argc, char **argv) { 
  int listenfd, connfd, clientfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {  
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다. */
  listenfd = Open_listenfd(argv[1]);

  /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit()호출 */
  while(1) {
    clientlen = sizeof(clientaddr);
    /* 클라이언트에게서 받은 연결 요청을 accept한다. connfd = proxy의 connfd*/
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    /* 연결이 성공했다는 메세지를 위해. Getnameinfo를 호출하면서 hostname과 port가 채워진다.*/
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    /* doit 함수를 실행 */
    do_it(connfd); 

    /* proxy의 connfd 연결 식별자를 닫아준다. */
    Close(connfd);
  }

  printf("%s", user_agent_hdr);

  return 0;
}

int do_it(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], host[MAXLINE], port[MAXLINE];
  char uri_ctop[MAXLINE], uri_ptos[MAXLINE];
  char new_version = 'HTTP/1.0';
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);            // rio 버퍼와 fd(proxy의 connfd)를 연결시켜준다. 
  Rio_readlineb(&rio, buf, MAXLINE);  // 그리고 rio(==proxy의 connfd)에 있는 한 줄(응답 라인)을 모두 buf로 옮긴다. 
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 각각 method, uri, version이라는 문자열에 저장 

  /* method가 GET인지 파악 */
  if (strcasecmp(method, "GET") ){ 
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return ;
  }

  /* read_requesthdrs: 요청 라인을 뺀 나머지 요청 헤더들을 무시(그냥 프린트)*/
  read_requesthdrs(&rio); 

  /* Parse URI from GET request */
  if (!(parse_uri(uri_ctop, uri_ptos, host, port)))
    return -1;
 
  clientfd = open_clientfd(host, port); // clientfd = proxy의 clientfd
  do_client(fd, clinetfd);

  Close(clientfd);


}

// do_client
// 1. from proxy의 client to server의 connfd
// 2. server의 connfd => proxy의 client
void do_client(int connfd, int clientfd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], host[MAXLINE], port[MAXLINE];
  char new_version = 'HTTP/1.0';

  rio_t rio;
  char buf[MAXLINE];

  /* Read request line */
  Rio_readinitb(&rio, clientfd);      // rio 버퍼와 fd(proxy의 connfd)를 연결시켜준다. 
  Rio_readlineb(&rio, buf, MAXLINE);  // 그리고 rio(==proxy의 connfd)에 있는 한 줄(응답 라인)을 모두 buf로 옮긴다. 
  printf("Request headers:\n");
  printf("%s", buf);
  sprintf(buf, "%s %s %s", method, uri, new_version);

  /* Read request headers */        
  sprintf(buf, "%sHost: %d\r\n", buf, ddd);  // Host 어떻게 받을 것인지?? clientfd에 있을건데 어케 빼먹지??       
  sprintf(buf, "%s%s", buf, user_agent_hdr); 
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sProxy-Connections: close\r\n\r\n", buf);


  // 내가 서버였을 때 받았던 애들을 일단 rio에 초기화해줘요 
  // Rio_readinitb(&rio, connfd);
  // 그걸 clientfd 적어주고 

  // 서버한테 요청해요
  // 서버에게 응답을 받아요

  // Close(clientfd);
  // 다시 connfd에 적어줘요 



}


/* 과제 조건: HTTP/1.0 GET 요청을 처리하는 기본 시퀀셜 프록시

  클라이언트가 프록시에게 다음 HTTP 요청 보내면 
  GET http://www.google.com:80/index.html HTTP/1.1 
  프록시는 이 요청을 파싱해야한다 ! 호스트네임, www.google.com, /index.html 
  
  이렇게 프록시는 서버에게 다음 HTTP 요청으로 보내야함.
  GET /index.html HTTP/1.1   
  
  즉, proxy는 HTTP/1.1로 받더라도 server는 HTTP/1.0으로 요청해야함 

  http://
  www.google.com
  :80
  /index.html
  
*/

int parse_uri(char *uri_ctop, char *uri_ptos, char *host, char *port){
  char *ptr;

 // strstr : 문자열의 처음 찾아지는 곳의 첫 번째 주소 반환
 // 만약 uri에 :// 이 없으면 strstr의 결과값이 NULL이 된다. => 예외처리를 해야함 
  if (!(ptr = strstr(uri_ctop, "://")))
    return -1;
  ptr += 3;
  strcpy(host, ptr);              // host = www.google.com:80/index.html

  if((ptr = strchr(host, ':'))){  // strchr(): 문자 하나만 찾는 함수 (''작은따옴표사용)
    *ptr = '\0';                  // host = www.google.com
    ptr += 1;
    strcpy(port, ptr);            // port = 80/index.html
  }
  else strcpy(port, "80"); 
  
  if ((ptr = strchr(port, "/"))){ // port = 80/index.html
    *ptr = '\0';                  // port = 80
    ptr += 1;     
    strcat(uri_ptos, '/');        // uri_ptos = /
    strcat(uri_ptos, ptr);        // uri_ptos = /index.html
  }  
  else strcat(uri_ptos, '/');

  return 0;
}

/* read_requesthdrs: 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시한다.(그냥 프린트)*/
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  /* 버퍼 rp의 마지막 끝을 만날 때까지 ("Content-length: %d\r\n\r\n에서 마지막 \r\n")*/
  /* 계속 출력해줘서 없앤다. */
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}