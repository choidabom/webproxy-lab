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
void get_filetype(char *filename, char *filetype);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void serve_static(int fd, char *filename, int filesize);
void serve_dynamic(int fd, char *filename, char *cgiargs);

/* port 번호를 인자로 받는다. */
int main(int argc, char **argv)
{ // argc: 인자 개수, argv: 인자 배열
  // argv는 main 함수가 받은 각각의 인자들
  // ./tiny 8000 => argv[0] = ./tiny, argv[1] => 8000
  // 메인함수에 전달되는 정보의 개수가 2개여야함 (argv[0]: 실행경로, argv[1]: port num)

  int listenfd, connfd; 
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;                  // clientaddr 구조체의 크기 (byte)
  struct sockaddr_storage clientaddr;   // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); // f(파일)printf => 파일을 읽어주는 것, stderr(식별자 2) = standard error
    exit(1);
  }

  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다. */
  listenfd = Open_listenfd(argv[1]); // argv[1]: port num

  /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit()호출 */
  while (1){    

    clientlen = sizeof(clientaddr);
    /* 클라이언트에게서 받은 연결 요청을 accept 한다. connfd = 서버 연결 식별자*/
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* 연결이 성공했다는 메세지를 위해. Getnameinfo를 호출하면서 hostname과 port가 채워진다.*/
    /* 마지막 부분은 flag로 0: 도메인 네임, 1: IP address */
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* doit 함수를 실행 */
    doit(connfd);

    /* 서버 연결 식별자를 닫아준다. */
    Close(connfd);
  }
}

/* doit: 클라이언트의 요청 라인을 확인해 정적, 동적 컨텐츠인지 확인하고 각각의 서버에 보낸다. */
void doit(int fd)
{
  int is_static;
  struct stat sbuf; // 한 번 더 찾아보기
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  /* 클라이언트가 rio로 보낸 요청 라인과 헤더를 읽고 분석한다. */
  Rio_readinitb(&rio, fd);                          // rio 버퍼와 fd, 여기서는 서버의 connfd를 연결시켜준다.
  Rio_readlineb(&rio, buf, MAXLINE);                // 그리고 rio(==connfd)에 있는 string 한 줄(Request line)을 모두 buf로 옮긴다.
  printf("Request headers:\n");
  printf("%s", buf);                                // Request line in buf => GET /godzilla.jpg HTTP/1.1
  sscanf(buf, "%s %s %s", method, uri, version);    // buf에서 문자열 3개를 읽어와 각각 method, uri, version이라는 문자열에 저장

  /* method가 GET인지 HEAD인지 파악 */
  /* strcasecmp(method, "GET") => method가 GET이면 return 0 => if문에 해당 x*/
  /* method가 GET이 아니라면 종료. main으로 가서 연결 닫고 다음 요청 기다림.*/
  if (strcasecmp(method, "GET") * strcasecmp(method, "HEAD"))
  { // 하나라도 0이면 0
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  /* read_requesthdrs: Request line을 뺀 나머지 요청 헤더들을 무시(그냥 프린트)*/
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  /* parse_uri: 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분한다.*/
  /* is_static이 1이면 정적 컨텐츠, 0이면 동적 컨텐츠 */
  is_static = parse_uri(uri, filename, cgiargs);

  /* 여기서 filename: 클라이언트가 요청한 서버의 컨텐츠 디렉토리 및 파일 이름 */
  if (stat(filename, &sbuf) < 0) 
  { // 파일이 없다면 404
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  /* Serve static content */
  if (is_static)
  {
    // !(일반 파일이다) or !(읽기 권한이 있다)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) // 버퍼가 사용가능 가능한 얘냐를 체크하는 것 
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    /* 정적 서버에 파일의 사이즈를 같이 보낸다. => Response header에 Content-length 위해 */
    serve_static(fd, filename, sbuf.st_size);
  }

  /* Serve dynamic content */
  else
  {
    // !(일반 파일이다) or !(실행 권한이 있다)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    // 동적 서버에 인자를 같이 보낸다.
    serve_dynamic(fd, filename, cgiargs);
  }
}

/* read_requesthdrs: 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시한다.(그냥 프린트)*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE); // Host: localhost:8000 => printf를 하지 않았기에 출력되지 않음

  /* 버퍼 rp의 마지막 끝을 만날 때까지 ("Content-length: %d\r\n\r\n에서 마지막 \r\n") */
  /* 계속 출력해줘서 없앤다. */
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* parse_uri: uri를 받아 요청받은 파일의 이름(filename)과 요청 인자(cgiarg)를 채워준다. */
/* cgiargs: 동적 컨텐츠의 실행 파일에 들어갈 인자 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  /* Static content */
  /* uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 리턴한다. */

  // 예시
  // Request Line: GET /godzilla.jpg HTTP/1.1
  // uri: /godzilla.jpg
  // cgiargs: x(없음)
  // filename: ./home.html

  if (!strstr(uri, "cgi-bin"))
  {
    strcpy(cgiargs, "");              // cgiargs는 동적 컨텐츠의 실행 파일에 들어갈 인자이기에 정적 컨텐츠의 경우 없다.
    strcpy(filename, ".");            // 현재 디렉토리에서 시작
    strcat(filename, uri);            // uri 넣어줌

    // 만약 uri 뒤에 '/'이 있다면 그 뒤에 home.html을 붙인다.
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }


  /* Dynamic content */
  /* uri에 cgi-bin이 있다면, 즉 동적 컨텐츠를 요청한다면 0을 리턴한다. */

  // 예시
  // Request Line: GET /cgi-bin/adder?15000&213 HTTP/1.0
  // uri: /cgi-bin/adder?123&123
  // cgiargs: 123&123
  // filename: ./cgi-bin/adder

  else
  {
    /* index: 문자를 찾았으면 문자가 있는 위치 반환 */
    /* ptr: '?'의 위치 */
    ptr = index(uri, '?');

    if (ptr)
    { /* '?'가 있으면 cgiargs를 '?' 뒤 인자들과 값으로 채워주고 ?를 NULL로 만든다. */
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else /* '?'가 없으면 그냥 아무것도 안 넣어줌. */
      strcpy(cgiargs, "");
    strcpy(filename, ".");            // 현재 디렉토리에서 시작
    strcat(filename, uri);            // uri 넣어줌
    return 0;
  }
}

/*get_filetype - Derive file type from filename */
void get_filetype(char *filename, char *filetype)
{
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

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body*/
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
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


void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);                         // 파일 이름의 접미어 부분 검사 => 파일 타입 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");                      // 응답 라인 작성
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);       // 응답 헤더 작성
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

  /* 응답 라인과 헤더를 클라이언트에게 보냄 */
  Rio_writen(fd, buf, strlen(buf)); 
  printf("Response headers: \n");
  printf("%s", buf);


  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);  // filename의 이름을 갖는 파일을 읽기 권한으로 불러온다.
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 메모리에 파일 내용을 동적할당한다.
  Close(srcfd);                         // 소켓을 열어놓는 것은 "치명적"인 메모리 누수 발생시킴 ..
  Rio_writen(fd, srcp, filesize);       // 해당 메모리에 있는 파일 내용들을 fd에 보낸다.(읽는다.)
  Munmap(srcp, filesize);               // 할당된 메모리 공간을 해제한다.

  // Homework 11.9: 정적 컨텐츠 처리할 때 요청 파일 malloc, rio_readn, rio_writen 사용하여 연결 식별자에게 복사
  // srcp = (char *)malloc(filesize);
  // rio_readn(srcfd, srcp, filesize);
  // Close(srcfd);
  // rio_writen(fd, srcp, filesize);
  // free(srcp);
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{ 
  // fork(): 함수를 호출한 프로세스를 복사하는 기능
  // 부모 프로세스(원래 진행되던 프로세스), 자식 프로세스(복사된 프로세스)
  // Tiny는 자식 프로세스를 fork하고, CGI 프로그램을 자식에서 실행하여 동적 컨텐츠를 표준 출력으로 보냄
  char buf[MAXLINE], *emptylist[] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) // fork() 자식 프로세스 생성됐으면 0을 반환 (성공)
  { 
    /* Return first part of HTTP response */
    // 환경변수를 cigarg로 바꿔주겠다 0: 기존 값 쓰겠다 . 1: cigargs 
    setenv("QUERY_STRING", cgiargs, 1);   
    // old file descriptor, new file descriptor
    // 화면에 출력할 수 있게끔 띄워주겠다 .
    Dup2(fd, STDOUT_FILENO);              // Redirect stdout to clinet
    Execve(filename, emptylist, environ); // Run CGI program
  }
  Wait(NULL); // Parent waits for and reaps child
}

/* 
  serve_dynamic 이해하기 
  1. fork()를 실행하면 부모 프로세스와 자식 프로세스가 동시에 실행된다.
  2. 만약 fork()의 반환값이 0이라면, 즉 자식 프로세스가 생성됐으면 if문을 수행한다. 
  3. fork()의 반환값이 0이 아니라면, 즉 부모 프로세스라면 if문을 건너뛰고 Wait(NULL) 함수로 간다. 이 함수는 부모 프로세스가 먼저 도달해도 자식 프로세스가 종료될 때까지 기다리는 함수이다.
  4. if문 안에서 setenv 시스템 콜을 수행해 "QUERY_STRING"의 값을 cgiargs로 바꿔준다. 우선순위가 1이므로 기존의 값과 상관없이 값이 변경된다.
  5. Dup2 함수를 실행해서 CGI 프로세스의 표준 출력을 fd(서버 연결 소켓 식별자)로 복사한다. 이제 STDOUT_FILENO의 값은 fd이다. 다시 말해, CGI 프로세스에서 표준 출력을 하면 그게 서버 연결 식별자를 거쳐 클라이언트에 출력된다.
  6. execuv 함수를 이용해 파일 이름이 filename인 파일을 실행한다.
*/