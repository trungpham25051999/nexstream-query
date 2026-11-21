#include "nexstream/query/infrastructure/http_server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <charconv>
#include <chrono>
#include <map>
#include <stdexcept>
#include <cerrno>
namespace nexstream::query::infrastructure {
namespace {
struct Fd {int value; ~Fd(){if(value>=0) close(value);} };
bool wait(int fd,short events,std::chrono::steady_clock::time_point deadline) {
    while(true) {auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count(); if(left<=0) return false; pollfd p{fd,events,0}; int n=poll(&p,1,static_cast<int>(left)); if(n<0&&errno==EINTR) continue; return n>0&&(p.revents&events);}
}
template<class T> bool number(std::string_view s,T& value) {auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),value); return ec==std::errc{}&&end==s.data()+s.size();}
std::optional<std::string> decode_path(std::string_view value) {
    auto digit=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    std::string decoded;
    for(size_t i=0;i<value.size();++i){if(value[i]!='%'){decoded+=value[i];continue;}if(i+2>=value.size())return {};int a=digit(value[i+1]),b=digit(value[i+2]);if(a<0||b<0)return {};decoded+=static_cast<char>(16*a+b);i+=2;}
    return decoded;
}
}
HttpServer::HttpServer(application::QueryService& s,std::uint16_t p,std::size_t max):service_(s),port_(p),max_request_bytes_(max) {}
HttpServer::~HttpServer(){stop();workers_.clear();for(const auto& client:pending_)close(client.first);if(listener_>=0) close(listener_);}
void HttpServer::stop() noexcept {stopped_=true;ready_.notify_all();}
void HttpServer::worker(){
    while(true){std::pair<int,std::chrono::steady_clock::time_point> client;
        {std::unique_lock lock(mutex_);ready_.wait(lock,[this]{return stopped_||!pending_.empty();});if(stopped_)return;client=pending_.front();pending_.pop_front();}
        Fd fd{client.first};try{handle_client(fd.value,client.second);}catch(const std::exception&){}
    }
}
void HttpServer::run() {
    listener_=socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0); if(listener_<0) throw std::runtime_error("socket");
    int yes=1; setsockopt(listener_,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)); sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=htons(port_);
    if(bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||listen(listener_,32)<0) throw std::runtime_error("listen");
    for(int i=0;i<4;++i)workers_.emplace_back([this]{worker();});
    while(!stopped_) {pollfd p{listener_,POLLIN,0}; if(poll(&p,1,100)<=0) continue; Fd client{accept4(listener_,nullptr,nullptr,SOCK_NONBLOCK|SOCK_CLOEXEC)};if(client.value<0)continue;
        int socket_budget=16384;
        if(setsockopt(client.value,SOL_SOCKET,SO_RCVBUF,&socket_budget,sizeof(socket_budget))||setsockopt(client.value,SOL_SOCKET,SO_SNDBUF,&socket_budget,sizeof(socket_budget)))continue;
        {std::lock_guard lock(mutex_);if(pending_.size()>=32){constexpr char busy[]="HTTP/1.1 503 Busy\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";(void)send(client.value,busy,sizeof(busy)-1,MSG_NOSIGNAL);continue;}pending_.emplace_back(client.value,std::chrono::steady_clock::now());client.value=-1;}
        ready_.notify_one();
    }
    ready_.notify_all();workers_.clear();
}
void HttpServer::handle_client(int fd,std::chrono::steady_clock::time_point accepted) {
    const auto deadline=accepted+std::chrono::seconds(3); std::string request; char buffer[1024];
    while(request.find("\r\n\r\n")==std::string::npos) {if(request.size()>=max_request_bytes_||!wait(fd,POLLIN,deadline)) return; auto n=recv(fd,buffer,std::min(sizeof(buffer),max_request_bytes_-request.size()),0); if(n<0&&(errno==EAGAIN||errno==EINTR)) continue; if(n<=0) return; request.append(buffer,static_cast<size_t>(n));}
    application::Response response{400,"{\"error\":\"invalid request\"}"};
    const auto end=request.find(" HTTP/1.1\r\n");
    if(request.starts_with("GET ")&&end!=std::string::npos) {
        std::string target=request.substr(4,end-4);
        if(target=="/health"||target=="/health/live") response=service_.health();
        else if(target=="/health/ready") response=service_.ready();
        else if(target.starts_with("/v1/hosts/")) {
            auto slash=target.find('/',10);
            if(slash!=std::string::npos) {
                auto decoded=decode_path(std::string_view(target).substr(10,slash-10));
                auto host=decoded.value_or(""); auto operation=target.substr(slash);
                if(operation=="/latest") response=service_.latest(host);
                else if(operation.starts_with("/metrics?")) {
                    std::map<std::string,std::string> args; size_t pos=9; bool valid=true;
                    while(pos<operation.size()) {auto next=operation.find('&',pos); auto part=operation.substr(pos,next-pos); auto eq=part.find('='); if(eq==std::string::npos||!args.emplace(part.substr(0,eq),part.substr(eq+1)).second) {valid=false;break;} if(next==std::string::npos) break; pos=next+1;}
                    domain::RangeRequest r; r.host_id=host;
                    for(const auto& [key,value]:args) { (void)value; if(key!="from"&&key!="to"&&key!="limit") valid=false; }
                    if(valid&&number(args["from"],r.from_ms)&&number(args["to"],r.to_ms)&&(!args.contains("limit")||number(args["limit"],r.limit))) response=service_.metrics(r);
                }
            }
        } else response={404,"{\"error\":\"not found\"}"};
    }
    std::string output="HTTP/1.1 "+std::to_string(response.status)+" Response\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+std::to_string(response.body.size())+"\r\n\r\n"+response.body;
    size_t sent=0; const auto send_deadline=accepted+std::chrono::seconds(6);
    while(sent<output.size()&&wait(fd,POLLOUT,send_deadline)) {auto n=send(fd,output.data()+sent,output.size()-sent,MSG_NOSIGNAL); if(n<0&&(errno==EINTR||errno==EAGAIN)) continue; if(n<=0) break; sent+=static_cast<size_t>(n);}
}
}
