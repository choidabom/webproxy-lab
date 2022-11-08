# Web Sever Lab
### SW Jungle Week07 (2022.11.03 ~ 11.10)
### WEEK07: BSD소켓, IP, TCP, HTTP, file descriptor, DNS

나만의 웹서버를 만들어보기 ! (프록시 서버까지)

- 클라이언트의 request를 받고, response를 내어주는 웹서버를 만들어봅니다. 웹 서버는 어떤 기능들의 모음일까요?
- '컴퓨터 시스템' 교재의 11장을 보면서 차근 차근 만들어주세요.
- 웹 서버를 완성했으면 프록시(proxy) 서버 과제에 도전합니다.
    - http://csapp.cs.cmu.edu/3e/proxylab.pdf
    - 출처: CMU (카네기멜론)
- 컴퓨터 시스템 교재의 9.9장을 찬찬히 읽어가며 진행하세요. 교재의 코드를 이해하고 옮겨써서 잘 동작하도록 실행해보는 것이 시작입니다!
- https://github.com/SWJungle/webproxy-jungle 의 내용대로 진행합니다.
- 진행방법
    - 책에 있는 코드를 기반으로, tiny 웹서버를 완성하기 (tiny/tiny.c, tiny/cgi-bin/adder.c 완성)
    - AWS 혹은 container 사용시 외부로 포트 여는 것을 잊지 말기
    - 숙제 문제 풀기 (11.6 c, 7, 9, 10, 11)
    - 프록시 과제 도전 (proxy.c 완성)
        - proxy I: sequential
        - proxy II: concurrent
        - proxy III: cache

## GOAL
- tiny 웹서버를 만들고 → 숙제문제 11.6c, 7, 9, 10, 11 중 세 문제 이상 풀기 → 프록시 과제 도전

## TIL (Today I Learned)
### 11.03 목

- CS:APP 11장 이해
    - 11.1 클라이언트-서버 프로그래밍 모델
    - 11.2 네트워크 
    - 11.3 글로벌 IP 인터넷

### 11.04 금

- CS:APP 11장 이해 
    - 11.3 글로벌 IP 인터넷
        - 11.3.1 IP 주소
        - 11.3.2 인터넷 도메인 이름(Internet Domain Name)
        - 11.3.3 인터넷 연결
    - 11.4 소켓 인터페이스
        - 11.4.1 소켓 주소 구조체
        
    - 11.5 웹 서버
        - 11.5.1 웹 기초
        - 11.5.2 웹 컨텐츠
        - 11.5.3 HTTP 트랜잭션
        - 11.5.4 동적 컨텐츠의 처리
    - 11.6 tiny 웹 서버
- tiny 웹 서버 이해

### 11.05 토

- tiny 웹 서버 구현 완료
- 숙제 11.6 c, 7, 9, 10, 11 

### 11.06 일

- proxy 과제 이해
- proxy.c 구현

### 11.07 월

- tiny.c 주석 추가
- parse_uri 구현 !

### 11.08 화

- 
- 

### 11.09 수

- 
- 


<details><summary style="color:skyblue">Introduction</summary>
Part 1. 
    들어오는 요청을 수학하고 읽고 파싱(해석 및 분할)한 후 서버에 요청을 전달하고, 다시 서버로부터 받은 응답을 읽고, 최종적으로 클라이언트에 전달하는 프록시를 만든다. 첫번째 파트는 기본적인 HTTP 오퍼레이션 네트워크 통신 프로그램을 구현하기 위해 소켓을 사용하는 방법을 익히는 파트이다.

Part 2. 
    여러분이 구현한 프록시가 다수의 동시적인 요청을 처리할 수 있도록 업그레이드하게 될 것이다. 이 과제를 통해 여러분은 매우 중요한 시스템 개념인 동시성을 다루게 될 것이다.

Part 3.
    프록시에 최근에 엑세스한 웹 컨텐츠를 캐싱하는 기능을 추가할 것이다. 
</details>



This directory contains the files you will need for the CS:APP Proxy Lab.

- proxy.c
- csapp.h
- csapp.c
    These are starter files.  csapp.c and csapp.h are described in
    your textbook. 

    You may make any changes you like to these files.  And you may
    create and handin any additional files you like.

    Please use `port-for-user.pl' or 'free-port.sh' to generate
    unique ports for your proxy or tiny server. 

- Makefile
    This is the makefile that builds the proxy program.  Type "make"
    to build your solution, or "make clean" followed by "make" for a
    fresh build. 

    Type "make handin" to create the tarfile that you will be handing
    in. You can modify it any way you like. Your instructor will use your
    Makefile to build your proxy from source.

- port-for-user.pl
    Generates a random port for a particular user
    usage: ./port-for-user.pl <userID>

- free-port.sh
    Handy script that identifies an unused TCP port that you can use
    for your proxy or tiny. 
    usage: ./free-port.sh

- driver.sh
    The autograder for Basic, Concurrency, and Cache.        
    usage: ./driver.sh

- nop-server.py
     helper for the autograder.         

- tiny
    Tiny Web server from the CS:APP text

#### Part 1. Implementing a sequential web proxy(한 번에 하나씩 요청을 처리하는 프록시)
첫번째 단계는 HTTP/1.0 요청을 처리하는 기본적인 sequential webproxy를 구현하는 것이다. POST는 선택사항.
 
1. proxy가 실행된 이후에는 프록시는 커맨드라인에 명시된 포트로부터 들어오는 요청을 항상 대기하고 있어야한다. 
2. 연결이 맺어지면 proxy는 클라이언트의 요청을 읽고 파싱(해석 및 분할)해야한다.
3. 그리고 클라이언트가 올바른 HTTP 요청을 보냈는지 검증해야 한다. 
4. 만약 올바른 요청으로 판단되면 적절한 웹 서버와 연결을 맺은 후 클라이언트로 받은 요청을 전달한다. 
5. 최종적으로 proxy는 서버의 응답을 읽고 클라이언트에 전달한다. 

##### 4.1 HTTP/1.0 GET requests
requests을 어떻게 처리하는가 
(클라이언트 단의) 사용자가 `https://www.cmu.edu/hub/index.html`과 같은 URI을 접속하면 **브라우저는 HTTP 요청을 proxy에 보낸다.**

- 요청 => `GET https://www.cmu.edu/hub/index.html HTTP/1.1`

그런데 proxy는 이 요청을 다음과 같은 필드 구성으로 파싱해야한다.
이렇게 함으로써 프록시는 www.cmu.edu에게 다음과 같은 HTTP 요청을 보내야함을 파악가능. 

- 파싱된 HTTP 요청 => `GET /hub/index.html HTTP/1.0`

명심하라.
HTTP 요청의 모든 라인들은 각각 맨 뒤에 리턴문자(\r)와 개행문자(\n)가 붙는다. 또한 모든 HTTP 요청은 반드시 '\r\n'으로 끝나야 한다.

위의 예시에서 프록시의 요청라인은 'HTTP/1.0'으로 끝나는 반면 웹브라우저의 요청라인은 'HTTP/1.1'로 끝난다는 것을 파악했을 것이다. 현대의 웹 브라우저들을 HTTP/1.1 요청을 생성하지만, 여러분의 프록시는 HTTP/1.0 요청으로 전달해야한다.

##### 4.2 Request headers (프록시가 서버한테 보낼 때)
이번 과제에서 중요한 요청 헤더 => `Host header`, `User-Agent header`, `Connection header`, `Proxy-Connection header`

- `Host: www.cmu.edu`
    => Always send a Host header.
- `User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3`
    => You may choose to always send User-Agent header (일단 보내자)
- `Connection: close` 
    => Always send Connection header
- `Proxy-Connection: close`
    => Always send Proxy-Connection header

Connection headers와 Proxy-Connection headers는 첫번재 요청/응답이 완료된 이후에도 지속적으로 살아있어야 있어야 하는지를 결정하는데 사용된다. 여러분의 프록시가 매번 요청을 받을 때마다 새로운 연결을 맺도록 권장한다. `close`로 헤더 값을 설정해놓으면 웹서버는 첫번재 요청/응답 이후 프록시와의 연결을 종료할 것이다.

##### 4.3 Port numbers
이번 과제에서 중요한 포트 클래스 => `HTTP request ports`, `proxy's listening ports`

- `HTTP request ports`는 `HTTP request` 중 URL에 있는 optional field이다.
- `http://www.cmu.edu:8080/hub/index.html` 이렇게 생긴 URI은 프록시가 `www.cmu.edu`라는 호스트와 연결을 맺었을 때 HTTP 기본 포트인 80 포트 대신에 8080 포트로 연결을 맺어야한다는 의미.
- 여러분의 프록시는 포트 번호가 URI에 포함되어있지 않든 정상적으로 동작해야한다.

- `proxy's listening ports`는 프록시가 자신에게 들어오는 연결 요청들을 대기하고 있는 포트이다.
- 프록시는 커맨드 라인 인자로 받은 listening ports를 수락해야한다. 예를 들어 다음과 같은 커맨드를 받으면 프록시는 15213 포트의 연결을 대기해야한다.

`linux> ./proxy 15213`

#### Part 2. Dealing with multiple concurrent requests
sequential proxy를 잘 구현했다면, 동시다발적인 요청을 처리할 수 있도록 해봐.
동시성 서버를 구현하는 가장 단순한 방법은 각각의 새로운 요청에 대해서 새로운 스레드를 생성하는 것.
교재 12.5.5에 나오는 prethreded(사전쓰레딩) 서버와 같은 다른 디자인패턴 사용해도 됨.

- threads는 메모리 누수를 피하기 위해서 반드시 detached mode로 실행되어야 한다.
- `open_clinetfd`와 `open_listenfd`함수는 현대적이며 프로토콜에 의존하지 않는 `getaddrinfo` 함수에 기반하고 있으므로 threads safe하다. 