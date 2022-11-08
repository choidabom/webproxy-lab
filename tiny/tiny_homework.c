/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char method_flag);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char method_flag);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/* port 번호를 인자로 받는다. */
int main(int argc, char **argv) { // argc: 인자 개수, argv: 인자 배열
  // argv는 main 함수가 받은 각각의 인자들
  // ./tiny 8000 => argv[0] = ./tiny, argv[1] => 8000 
  // 메인함수에 전달되는 정보의 개수가 2개여야함 (argv[0]: 실행경로, argv[1]: port num)

  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  /* 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소*/
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) { 
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다. */
  listenfd = Open_listenfd(argv[1]); // argv[1]: port num

  /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit()호출 */
  while (1) { 
    /* 클라이언트에게서 받은 연결 요청을 accept 한다. connfd = 서버 연결 식별자*/
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* 연결이 성공했다는 메세지를 위해. Getnameinfo를 호출하면서 hostname과 port가 채워진다.*/
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* doit 함수를 실행 */
    doit(connfd);   

    /* 서버 연결 식별자를 닫아준다. */
    Close(connfd);  
  }
}

/* doit: 클라이언트의 요청 라인을 확인해 정적, 동적 컨텐츠인지 확인하고 각각의 서버에 보낸다. */
void doit(int fd){
  int is_static;
  struct stat sbuf;
  char method_flag; // Homework 11.11: 0: GET, 1: HEAD
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  
  /* Read request line and headers */
  /* 클라이언트가 rio로 보낸 요청 라인과 헤더를 읽고 분석한다. */
  Rio_readinitb(&rio, fd);            // rio 버퍼와 fd, 여기서는 서버의 connfd를 연결시켜준다.
  Rio_readlineb(&rio, buf, MAXLINE);  // 그리고 rio(==connfd)에 있는 string 한 줄(응답 라인)을 모두 buf로 옮긴다.
  printf("Request headers:\n");
  printf("%s", buf);                  // 요청 라인 buf => GET / HTTP/1.1
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 각각 method, uri, version이라는 문자열에 저장

  /* method가 GET인지 HEAD인지 파악 */
  if (strcasecmp(method, "GET") * strcasecmp(method, "HEAD")){ // 하나라도 0이면 0
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return ;
  }
  /* read_requesthdrs: 요청 라인을 뺀 나머지 요청 헤더들을 무시(그냥 프린트)*/
  read_requesthdrs(&rio); 


  if (strcasecmp(method, "GET") == 0) method_flag = 0; 
  else method_flag = 1;

  /* Parse URI from GET request */
  /* parse_uri: 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분한다.*/
  /* is_static이 1이면 정적 컨텐츠, 0이면 동적 컨텐츠 */
  is_static = parse_uri(uri, filename, cgiargs); 
  /* 여기서 filename: 클라이언트가 요청한 서버의 컨텐츠 디렉토리 및 파일 이름 */
  if (stat(filename, &sbuf) < 0){ // 파일이 없다면 404
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return ;
  }

  /* Serve static content */
  if (is_static) {
    // !(일반 파일이다) or !(읽기 권한이 있다)
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return ;
    }
    /* 정적 서버에 파일의 사이즈를 같이 보낸다. => Response header에 Content-length 위해 */
    serve_static(fd, filename, sbuf.st_size, method_flag);
  }

  /* Serve dynamic content */
  else {
    // !(일반 파일이다) or !(실행 권한이 있다)
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return ;
    }
    // 동적 서버에 인자를 같이 보낸다. 
    serve_dynamic(fd, filename, cgiargs, method_flag); 
  }

}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web Server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));

  // 에러 메시지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다. 
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
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

/* parse_uri: uri를 받아 요청받은 파일의 이름(filename)과 요청 인자(cgiarg)를 채워준다. */
/* cgiargs: 동적 컨텐츠의 실행 파일에 들어갈 인자 */
int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  /* Static content */
  /* uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 리턴한다. */
  if (!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");    // strcpy => return a pointer to the destination string dest
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }

  // 예시
  // Request Line: GET /godzilla.jpg HTTP/1.1
  // uri: /godzilla.jpg
  // cgiargs: x(없음)
  // filename: ./home.html


  /* Dynamic content */
  /* uri에 cgi-bin이 있다면, 즉 동적 컨텐츠를 요청한다면 0을 리턴한다. */
  else {
    /* index: 문자를 찾았으면 문자가 있는 위치 반환 */ 
    /* ptr: '?'의 위치 */
    ptr = index(uri, '?'); 
    
    if (ptr){ /* '?'가 있으면 cgiargs를 '?' 뒤 인자들과 값으로 채워주고 ?를 NULL로 만든다. */
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else  /* '?'가 없으면 그냥 아무것도 안 넣어줌. */
      strcpy(cgiargs, "");
    strcpy(filename, "."); // 현재 디렉토리에서 시작
    strcat(filename, uri); // uri 넣어줌
    return 0;
  }

  // 예시
  // Request Line: GET /cgi-bin/adder?15000&213 HTTP/1.0
  // uri: /cgi-bin/adder?123&123
  // cgiargs: 123&123
  // filename: ./cgi-bin/adder
}

void serve_static(int fd, char *filename, int filesize, char method_flag){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client: 클라이언트에게 응답 헤더 보내기 */
  get_filetype(filename, filetype); // 파일 이름의 접미어 부분 검사 => 파일 타입 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                        // 응답 라인 작성
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);         // 응답 헤더 작성
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  /* 응답 라인과 헤더를 클라이언트에게 보냄 */
  Rio_writen(fd, buf, strlen(buf));   
  printf("Response headers: \n");     
  printf("%s", buf);

  if (method_flag) return;

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);

  // Homework 11.9: 정적 컨텐츠 처리할 때 요청 파일 malloc, rio_readn, rio_writen 사용하여 연결 식별자에게 복사
  // srcp = (char *)malloc(filesize);
  // rio_readn(srcfd, srcp, filesize);
  // Close(srcfd);
  // rio_writen(fd, srcp, filesize);
   // free(srcp);

}

/*get_filetype - Derive file type from filename */
void get_filetype(char *filename, char *filetype){
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");

  // Homework 11.7: html5 not supporting "mpg file format"
  else if (strstr(filename, ".mpg")) 
    strcpy(filetype, "video/mpg");
  else if (strstr(filename, ".mp4")) 
    strcpy(filetype, "video/mp4");
  else 
    strcpy(filetype, "text/plain");

}

void serve_dynamic(int fd, char *filename, char *cgiargs, char method_flag){
  char buf[MAXLINE], *emptylist [] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { // Child
    if (method_flag) setenv("REQUEST_METHOD", cgiargs, 1);
    setenv("QUERY_STRING", cgiargs, 1);

    Dup2(fd, STDOUT_FILENO);                // Redirect stdout to clinet
    Execve(filename, emptylist, environ);   // Run CGI program
  }
  Wait(NULL); // Parent waits for and reaps child
}

// 내 ip 주소: 143.248.222.78