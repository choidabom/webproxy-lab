#include <stdio.h>
#include "csapp.h"

/* 과제 조건: HTTP/1.0 GET 요청을 처리하는 기본 시퀀셜 프록시

  클라이언트가 프록시에게 다음 HTTP 요청 보내면 
  GET http://www.google.com:80/index.html HTTP/1.1 
  프록시는 이 요청을 파싱해야한다 ! 호스트네임, www.google.com, /index.html 
  
  이렇게 프록시는 서버에게 다음 HTTP 요청으로 보내야함.
  GET /index.html HTTP/1.0   
  
  즉, proxy는 HTTP/1.1로 받더라도 server에는 HTTP/1.0으로 요청해야함 
*/

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *new_version = "HTTP/1.0";

/* Function prototyps => 함수 순서 상관이 없게됨 */
void do_it(int fd);
void do_request(int p_clientfd, char *method, char *uri_ptos, char *host);
void do_response(int p_connfd, int p_clientfd);
int parse_uri(char *uri, char *uri_ptos, char *host, char *port);
int parse_responsehdrs(rio_t *rp, int length);

int main(int argc, char **argv) { 
  int listenfd, p_connfd;
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
    /* 클라이언트에게서 받은 연결 요청을 accept한다. p_connfd = proxy의 connfd*/
    p_connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* 연결이 성공했다는 메세지를 위해. Getnameinfo를 호출하면서 hostname과 port가 채워진다.*/
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    do_it(p_connfd); 
    Close(p_connfd);
  }
  return 0;
}

  /* 
  Before Parsing (Client로부터 받은 Request Line)
  => GET http://www.google.com:80/index.html HTTP/1.1

  Result Parsing
  => host = www.google.com
  => port = 80
  => uri_ptos = /index.html

  After Parsing (Server로 보낼 Request Line)
  => GET /index.html HTTP/11. 
  */ 

void do_it(int p_connfd){
  int p_clientfd;
  char buf[MAXLINE],  host[MAXLINE], port[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char uri_ptos[MAXLINE];
  rio_t rio;
  
  /* Read request line and headers from Client */
  Rio_readinitb(&rio, p_connfd);            // rio 버퍼와 fd(proxy의 connfd)를 연결시켜준다. 
  Rio_readlineb(&rio, buf, MAXLINE);  // 그리고 rio(==proxy의 connfd)에 있는 한 줄(응답 라인)을 모두 buf로 옮긴다. 
  printf("Request headers to proxy:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 각각 method, uri, version이라는 문자열에 저장 

  /* Parse URI from GET request */
  // if (!(parse_uri(uri, uri_ptos, host, port)))
  //   return -1;
  parse_uri(uri, uri_ptos, host, port);

  p_clientfd = Open_clientfd(host, port);             // p_clientfd = proxy의 clientfd (연결됨)
  do_request(p_clientfd, method, uri_ptos, host);     // p_clientfd에 Request headers 저장과 동시에 server의 connfd에 쓰여짐
  do_response(p_connfd, p_clientfd);        
  Close(p_clientfd);                                  // p_clientfd 역할 끝
}

/* do_request: proxy => server */
void do_request(int p_clientfd, char *method, char *uri_ptos, char *host){
  char buf[MAXLINE];
  printf("Request headers to server: \n");     
  printf("%s %s %s\n", method, uri_ptos, new_version);

  /* Read request headers */        
  sprintf(buf, "GET %s %s\r\n", uri_ptos, new_version);     // GET /index.html HTTP/1.0
  sprintf(buf, "%sHost: %s\r\n", buf, host);                // Host: www.google.com     
  sprintf(buf, "%s%s", buf, user_agent_hdr);                // User-Agent: ~(bla bla)
  sprintf(buf, "%sConnections: close\r\n", buf);            // Connections: close
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);   // Proxy-Connection: close

  /* Rio_writen: buf에서 p_clientfd로 strlen(buf) 바이트로 전송*/
  Rio_writen(p_clientfd, buf, (size_t)strlen(buf)); // => 적어주는 행위 자체가 요청하는거야~@!@!
}

/* do_response: server => proxy */
void do_response(int p_connfd, int p_clientfd){
  char buf[MAX_CACHE_SIZE];
  ssize_t n;
  rio_t rio;

  Rio_readinitb(&rio, p_clientfd);  //
  n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);   // robust ...~ MAXLINE까지 일단 다 읽음 
  Rio_writen(p_connfd, buf, n);
}

/* parse_uri: Parse URI from (Client로 부터 받은) GET request, Proxy에서 Server로의 GET request를 하기 위해 필요 */
int parse_uri(char *uri, char *uri_ptos, char *host, char *port){ 
  char *ptr;

  if (!(ptr = strstr(uri, "://")))
    return -1;
  ptr += 3;                       
  strcpy(host, ptr);              // host = www.google.com:80/index.html

  if((ptr = strchr(host, ':'))){  // strchr(): 문자 하나만 찾는 함수 (''작은따옴표사용)
    *ptr = '\0';                  // host = www.google.com
    ptr += 1;
    strcpy(port, ptr);            // port = 80/index.html
  }
  else{
    if((ptr = strchr(host, '/'))){
      *ptr = '\0';
      ptr += 1;
    }
    strcpy(port, "80"); 
  }

  if ((ptr = strchr(port, '/'))){ // port = 80/index.html
    *ptr = '\0';                  // port = 80
    ptr += 1;     
    strcpy(uri_ptos, "/");        // uri_ptos = /
    strcat(uri_ptos, ptr);        // uri_ptos = /index.html
  }  
  else strcpy(uri_ptos, "/");

  return 0; // function int return => for valid check
}