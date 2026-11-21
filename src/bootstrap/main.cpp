#include "nexstream/query/infrastructure/clickhouse_client.hpp"
#include "nexstream/query/infrastructure/http_server.hpp"
#include <iostream>
#include <csignal>
#include <pthread.h>
#include <thread>
int main(int argc,char** argv) {
    sigset_t signals;sigemptyset(&signals);sigaddset(&signals,SIGINT);sigaddset(&signals,SIGTERM);
    if(pthread_sigmask(SIG_BLOCK,&signals,nullptr))return 1;
    try {nexstream::query::infrastructure::ClickHouseClient db(argc>1?argv[1]:"http://127.0.0.1:8123",std::chrono::milliseconds(500),std::chrono::milliseconds(2500)); nexstream::query::application::QueryService service(db); nexstream::query::infrastructure::HttpServer server(service,8080);
        std::jthread signal_thread([&](std::stop_token stop){while(!stop.stop_requested()){timespec timeout{0,100000000};int received=sigtimedwait(&signals,nullptr,&timeout);if(received==SIGINT||received==SIGTERM){server.stop();return;}}});
        server.run();
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
