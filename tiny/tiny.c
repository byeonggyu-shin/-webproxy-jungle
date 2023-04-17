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
void serve_static(int fd, char *filename, int filesize, const char* method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, const char* method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* 
* main - 웹 서버 초기화, 지정된 포트에서 수신 소켓 열고 요청을 처리 후 연결을 닫는 무한 루프
*/
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

/*
*  doit - 개별 HTTP 요청을 처리, 요청이 정적, 동적인지 판단해서 응답 제공
*/
void doit(int fd) {
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
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return; 
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn’t find this file");
    return; 
  }

  if (is_static) { /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t read the file");
      return; 
    }
    serve_static(fd, filename, sbuf.st_size, method);
  } else { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
      return; 
    }
    serve_dynamic(fd, filename, cgiargs, method);
  }
}

/**
*  clienterror - 클라이언트에 HTTP 오류 응답
*  @param fd 파일 설명자
*  @param cause 오류 원인
*  @param errnum 오류 번호 
*  @param shortmsg 짧은 오류 메시지
*  @param longmsg  긴 오류 메시지
**/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/*
*  read_requesthdrs - robust I/O(RIO) 버퍼를 사용하여 HTTP 요청 헤더를 읽고 무시
*/
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];
  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return; 
}

/*
* parse_uri - HTTP 요청에서 URI를 구문 분석하여 요청된 파일 이름 및 CGI 인수(있는 경우)를 추출, 요청이 정적 또는 동적 콘텐츠에 대한 것인지 결정
*/
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {  /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/'){
        strcat(filename, "home.html");
    }
    return 1; 
  } 
  else {  /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0'; 
    } 
    else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// /*
// * serve_static - 적절한 HTTP 응답 헤더와 요청된 파일의 내용을 클라이언트로 전송하여 정적 콘텐츠를 제공
// */
void serve_static(int fd, char *filename, int filesize, const char* method) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  // printf("Response headers:\n");
  // printf("%s", buf);

  /* Send response body to client */
  if (strcasecmp(method, "GET") == 0) {
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
  }
}

// /*
// * serve_static - 적절한 HTTP 응답 헤더와 요청된 파일의 내용을 클라이언트로 전송하여 정적 콘텐츠를 제공
// * malloc, rio_readn, rio_writen 으로 연결식별자에게 복사
// */
// void serve_static(int fd, char *filename, int filesize) {
//   int srcfd;
//   char *bufp, filetype[MAXLINE], buf[MAXBUF];

//   // 클라이언트에 응답 헤더 보내기 
//   get_filetype(filename, filetype);
//   sprintf(buf, "HTTP/1.0 200 OK\r\n");
//   sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
//   sprintf(buf, "%sConnection: close\r\n", buf);
//   sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
//   sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
//   Rio_writen(fd, buf, strlen(buf));
//   printf("Response headers:\n");
//   printf("%s", buf);

//   // 클라이언트에 응답 본문 보내기 
//   srcfd = Open(filename, O_RDONLY, 0);
  
//   // 파일 내용 메모리 할당
//   bufp = malloc(filesize);
//   if (!bufp) {
//     fprintf(stderr, "Failed to allocate memory\n");
//     return;
//   }
  
//   // 할당된 메모리에 파일 내용 읽기
//   Rio_readn(srcfd, bufp, filesize);
  
//   // 연결된 파일 설명자에 파일 내용 쓰기
//   Rio_writen(fd, bufp, filesize);
  
//   // 할당된 메모리를 해제하고 원본 파일 설명자를 닫기
//   free(bufp);
//   Close(srcfd);
// }

/*
* get_filetype - 확장명에 따라 요청된 파일의 MIME 파일 형식을 결정, HTTP 응답 헤더에 사용
*/
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")){
    strcpy(filetype, "text/html");
  }
  else if (strstr(filename, ".gif")){
    strcpy(filetype, "image/gif");
  }
  else if (strstr(filename, ".png")){
    strcpy(filetype, "image/png");
  }
  else if (strstr(filename, ".jpg")){
    strcpy(filetype, "image/jpeg");
  }
  else if (strstr(filename, ".mp4")) { 
    strcpy(filetype, "video/mp4");      // Set MIME type for MP4 files
  }
  else {
    strcpy(filetype, "text/plain");
  }~
}

/*
* serve_dynamic - CGI(Common Gateway Interface) 프로그램에서 생성된 동적 콘텐츠를 제공 
* 적절한 HTTP 응답 헤더를 보내고 CGI 프로그램에 대한 환경을 설정한 후 실행하여 프로그램의 출력을 클라이언트로 보냄
*/
void serve_dynamic(int fd, char *filename, char *cgiargs, const char* method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */

  /* Send response headers to client, but only for GET requests */
  if (strcasecmp(method, "HEAD") != 0) {
      sprintf(buf, "Connection: close\r\n");
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Content-length: %d\r\n", strlen(buf));
      Rio_writen(fd, buf, strlen(buf));
      sprintf(buf, "Content-type: text/html\r\n\r\n");
      Rio_writen(fd, buf, strlen(buf));
  }
}