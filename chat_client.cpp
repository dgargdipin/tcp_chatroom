#include<ncurses.h>
#include<string>
#include <cstddef>
#include <unistd.h>
#include <cstdio>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include<iostream>
#include <arpa/inet.h>
#include <signal.h>
#include<thread>
#include<mutex>
using namespace std;

class ClientWin{
        WINDOW *stdscr,*inp_win;
        mutex write_mtx;
        int inp_win_height=3;
        public:
        ~ClientWin(){
                endwin();
        }
        void write(string a){
                lock_guard<mutex> lck(write_mtx);
                int y,x;
                getyx(stdscr,y,x);

                mvwprintw(stdscr,++y,0,"> ");
                printw(a.c_str());
                refresh();
               
        };
        string read(){
                char str[200];
                wmove(inp_win,1,1);
                wclrtoeol(inp_win);
                wrefresh(inp_win);
                mvwgetstr(inp_win,1,1,str);
                wmove(inp_win,1,1);
                wclrtoeol(inp_win);
                wrefresh(inp_win);
                return string(str);
        };
        void run(){
                stdscr=initscr();
                refresh();
                cbreak();
                int starty=(LINES-inp_win_height);
                inp_win=newwin(inp_win_height,COLS,starty,0);
                box(inp_win,0,0);
                wrefresh(inp_win);
        }




};

class TCP_CLIENT{
                int sockfd;
                struct addrinfo hints,*servinfo,*addr_iter;
                const char *hostname,*port;
                ClientWin *out;
                int MAXDATASIZE;
        public:
                TCP_CLIENT(string host,string port,ClientWin *out,int max_datasize=100):hostname(host.c_str()),port(port.c_str()),out(out),MAXDATASIZE(max_datasize){
                        memset(&hints,0,sizeof hints);
                        hints.ai_family=AF_UNSPEC;
                        hints.ai_socktype=SOCK_STREAM;
                        int addrinfo_retvalue;
                        if((addrinfo_retvalue=getaddrinfo(hostname,this->port,&hints,&servinfo))!=0){
                                fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(addrinfo_retvalue));
                                exit(1);
                        }

                };
                ~TCP_CLIENT(){
                        freeaddrinfo(servinfo);
                        close(sockfd);
                }
                void *get_in_addr(struct sockaddr *sa)
                {
                        if (sa->sa_family == AF_INET) {
                                return &(((struct sockaddr_in*)sa)->sin_addr);
                        }

                        return &(((struct sockaddr_in6*)sa)->sin6_addr);
                }
                void connectToServer(){
                        for(addr_iter=servinfo;addr_iter!=NULL;addr_iter=addr_iter->ai_next){
                                if((sockfd=socket(addr_iter->ai_family,addr_iter->ai_socktype,addr_iter->ai_protocol))==-1){
                                        out->write(strerror(errno));
                                        continue;
                                }
                                if(connect(sockfd,addr_iter->ai_addr,addr_iter->ai_addrlen)==-1){
                                        close(sockfd);
                                        out->write(strerror(errno));
                                        continue;
                                }
                                break;

                        }
                        if(addr_iter==NULL){
                                fprintf(stderr, "client: failed to connect\n");
                                exit(1);

                        }
                        char s[INET6_ADDRSTRLEN];
                        inet_ntop(addr_iter->ai_family,get_in_addr((sockaddr*)addr_iter->ai_addr),s,sizeof s);
                        char connnected_msg[100];
                        sprintf(connnected_msg,"client: connected to %s\n", s);
                        out->write(connnected_msg);

                }


                string receive(){
                                char buffer[MAXDATASIZE];
                                int numbytes;
                                if((numbytes=recv(sockfd,buffer,MAXDATASIZE-1,0))==-1){
                                        perror("recv");
                                        exit(1);
                                }
                                buffer[numbytes]='\0';
                                return string(buffer);
                }

                void sendMsg(string msg){
                        int numbytes;
                        const char* str=msg.c_str();
                        if (send(sockfd,str,strlen(str), 0) == -1){
                                        out->write(strerror(errno));
                        }
                }



};

class ClientApp{
        TCP_CLIENT *client;
        ClientWin *window;
        public:
        ClientApp(string hostname,string port){
                window=new ClientWin();
                client=new TCP_CLIENT(hostname,port,window);
                window->run();
                client->connectToServer();
                thread receiver(&ClientApp::receive_and_print,this);
                thread inp_send(&ClientApp::input_and_send,this);
                inp_send.join();
                receiver.join();
        }
        void input_and_send(){
                window->write("Please enter your name");
                auto name=window->read();
                client->sendMsg(name);
                while(1){
                        auto msg=window->read();
                        // cerr<<msg<<endl;
                        client->sendMsg(msg);
                }
        }
        void receive_and_print(){
                while(1){
                        window->write(client->receive());
                }

        }
        ~ClientApp(){
            
                free( window);
                free( client);

        }
};


int main(int argc,char **argv){
        // string hostname="127.0.0.1";
        // string port="3491";
        if (argc != 3) {
          printf("Usage: %s <hostname> <port>\n", argv[0]);
          exit(1);
        }
        string hostname(argv[1]);
        string port(argv[2]);
        ClientApp(hostname,port);
}
