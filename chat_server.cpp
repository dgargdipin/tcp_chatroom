
#include <future>
#include <arpa/inet.h>
#include <chrono>
#include <ctime>
#include <errno.h>
#include <functional>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <ostream>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include<iostream>
#include<algorithm>
#include<unordered_map>
#define BACKLOG 10

class Logger{
        std::ostream *out;
        std::mutex _mtx;
        bool _owner{false};
        public:
        enum typelog {
                DEBUG,
                INFO,
                WARN,
                ERROR,
                MESSAGE

        };

        virtual ~Logger()
        {
                setStream(0, false);
        }

        void setStream(std::ostream *out,bool owner){
                std::lock_guard<std::mutex> lck(_mtx);
                if(_owner) delete this->out;
                this->out=out;
                _owner=owner;
        }
        template<typename T>
                void  log(typelog type,const T& obj){
                        std::string type_string;
                        switch(type){
                                case typelog::DEBUG:
                                        type_string="DEBUG: ";
                                        break;
                                case typelog::ERROR:
                                        type_string="ERROR: ";
                                        break;
                                case typelog::INFO:
                                        type_string="INFO: ";
                                        break;
                                case typelog::WARN:
                                        type_string="WARN: ";
                                        break;
                                case typelog::MESSAGE:
                                        type_string="MESSAGE: ";
                                        break;
                        }
                        std::string out_str;
                        std::stringstream buffer;
                        buffer<<type_string<<obj;
                        this->operator<<(buffer.str());



                }
        template<typename T >
                Logger& operator<<(const T& obj){

                        std::lock_guard<std::mutex> lck(_mtx);
                        if(!out)
                                throw std::runtime_error("No stream set for Logger class");
                        
                        (*out) << obj<<std::endl;
                        return *this;

                }
};




class TCP_Server{
        int sockfd;
        const char *port;
        struct addrinfo hints, *servinfo;
        bool error_state;
        std::mutex io_mtx,threadMutex,socket_store_mtx;
        std::vector<std::thread> _threads;
        Logger logger;
        std::unordered_map<std::thread::id,int> socket_store;
        void fill_addrinfo(){
                memset(&hints,0,sizeof hints);
                hints.ai_family=AF_UNSPEC;
                hints.ai_socktype=SOCK_STREAM;
                hints.ai_flags=AI_PASSIVE;
                int rv;
                if ((rv = getaddrinfo(NULL,port, &hints, &servinfo)) != 0) {
                        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                        exit(1);
                };

        }
        void bind_socket(){
                addrinfo *p;
                for(p=servinfo;p!=NULL;p=p->ai_next){
                        if((sockfd=socket(p->ai_family,p->ai_socktype,p->ai_protocol))==-1){
                                perror("server: socket");
                                continue;
                        }
                        int yes=1;
                        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                                                sizeof(int)) == -1) {
                                perror("setsockopt");
                                exit(1);
                        }
                        if(bind(sockfd,p->ai_addr,p->ai_addrlen)==-1){
                                close(sockfd);
                                perror("server: bind");
                                continue;

                        }
                        break;

                }
                if(p==NULL){
                        fprintf(stderr, "server: failed to bind\n");
                        exit(1);

                }
        }
        void *get_in_addr(struct sockaddr *sa)
        {
                if (sa->sa_family == AF_INET) {
                        return &(((struct sockaddr_in*)sa)->sin_addr);
                }

                return &(((struct sockaddr_in6*)sa)->sin6_addr);
        }
        public:
        TCP_Server(int port){
                logger.setStream(&std::cout,0);
                this->port = std::to_string(port).c_str();
                this->fill_addrinfo();
                this->bind_socket();
        }
        void listen(){
                if (::listen(sockfd, BACKLOG) == -1) {
                        perror("listen");
                        exit(1);
                }

        }
        void accept_connections(){
                while(1){
                        sockaddr_storage their_addr;
                        socklen_t sin_size=sizeof their_addr;
                        int new_fd=accept(sockfd,(sockaddr*)&their_addr,&sin_size);
                        if (new_fd == -1) {
                                logger.log(Logger::typelog::ERROR,strerror(errno));
                                continue;
                        }
                        char s[INET6_ADDRSTRLEN];
                        inet_ntop(their_addr.ss_family,get_in_addr((sockaddr*)&their_addr),s,sizeof s);
                        char info_str[200];
                        sprintf(info_str,"server: got connection from %s with fd : %d", s,new_fd);
                        logger.log(Logger::typelog::INFO,info_str);
                        std::lock_guard<std::mutex> lock(threadMutex);

                        _threads.emplace_back(std::thread(&TCP_Server::handleConnection,this,new_fd,*((sockaddr*)&their_addr),s));


                }
        }

        void handleConnection(int io_fd,sockaddr their_addr,const char *host){
                char name_buffer[100];
                int bytesRead=recv(io_fd,name_buffer,sizeof name_buffer,0);

                if(bytesRead==0){
                        sprintf(name_buffer,"%s: coult not receive name",host);
                        logger.log(Logger::typelog::ERROR,name_buffer);
                }
                std::string name_str(name_buffer);
                std::string broadcast_str=name_str+" has joined the chat";
                socket_store_mtx.lock();
                socket_store[std::this_thread::get_id()]=io_fd;
                socket_store_mtx.unlock();

                logger.log(Logger::typelog::INFO,broadcast_str);
                broadcast(broadcast_str);
                while(1){
                        char buffer[2000];
                        bytesRead=recv(io_fd,buffer,sizeof buffer,0);
                        if(bytesRead==0){
                                broadcast_str=name_str+" has left the chat";
                                logger.log(Logger::typelog::INFO,broadcast_str);
                                broadcast(broadcast_str);
                                break;
                        }
                        else{
                                buffer[bytesRead]='\0';
                                broadcast_str=name_str+":"+std::string(buffer);
                                logger.log(Logger::typelog::MESSAGE,broadcast_str);
                                broadcast(broadcast_str);

                        }


                }

                close(io_fd);
                socket_store_mtx.lock();
                socket_store.erase(std::this_thread::get_id());
                socket_store_mtx.unlock();
                std::thread(&TCP_Server::removeThread, this,std::this_thread::get_id()).detach();



        }
        void removeThread(std::thread::id id)
        {
                std::lock_guard<std::mutex> lock(threadMutex);
                auto iter = std::find_if(_threads.begin(), _threads.end(), [=](std::thread &t) { return (t.get_id() == id); });
                if (iter != _threads.end())
                {
                        iter->detach();
                        _threads.erase(iter);
                }
        }

        void broadcast(std::string message){
                for(auto iter=_threads.begin();iter!=_threads.end();iter++){
                        auto store_iter=socket_store.find(iter->get_id());
                        if(store_iter!=socket_store.end()){
                                std::thread(&TCP_Server::broadcast_to, this,store_iter->second,message).detach();
                        }
                }

        }
        void broadcast_to(int fd,std::string message){
                const char *str=message.c_str();
                send(fd,str,strlen(str),0);
        }

};


int main(){
        TCP_Server server(3491);
        server.listen();
        server.accept_connections();
}
