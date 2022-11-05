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
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) { // 메인함수에 전달되는 정보의 개수가 2개여야함 (argv[0]: 실행경로, argv[1]: port)
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // argv[1]: port num
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    // SA: socket address clientaddr __addr_len)

    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // hostname이 채워짐
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  
  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET")){
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return ;
  }
  read_requesthdrs(&rio); // 다른 요청 헤더들을 무시

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return ;
  }

  /* Serve static content */
  if (is_static) {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return ;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  /* Serve dynamic content */
  else {
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return ;
    }
    serve_dynamic(fd, filename, cgiargs); 
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
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  /* Static content */
  if (!strstr(uri, "cgi-bin")){
    strcpy(cgiargs, "");    // strcpy => return a pointer to the destination string dest
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  /* Dynamic content */
  else {
    ptr = index(uri, '?');
    if (ptr){
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else  
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype); // 파일 이름의 접미어 부분 검사 => 파일 타입 결정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnections: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers: \n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);

  
  /* Mmap(): creates a new mapping in the virtual address space of the calling process */
  /* MAP_PRIVATE: indicate that this object(file) is ans "copy-on-write" one. (for Rio_writen)*/
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);

  // srcfd라는 파일에 적혀있는 정보들을 srcp라는 버퍼에 읽어오는거야 . 
  // srcp 버퍼에서 읽어와서 fd에 적어주는거야. 
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

void serve_dynamic(int fd, char *filename, char *cgiargs){
  char buf[MAXLINE], *emptylist [] = {NULL};

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // CGI 실행할 코드 파일

  if (Fork() == 0) { // Child
    /* Real server would set all CGI vars here */ 
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO);                // Redirect stdout to clinet
    Execve(filename, emptylist, environ);   // Run CGI program
  }
  Wait(NULL); // Parent waits for and reaps child
}