#include "nexstream/query/application/query_service.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace nexstream::query;
struct Repository:application::IMetricsRepository {
    int calls=0; bool failed=false;
    std::vector<domain::MetricRow> range(const domain::RangeRequest&) override {++calls;if(failed) throw std::runtime_error("offline");return {{"host","id",1,2,50,10,20}};}
    std::optional<domain::MetricRow> latest(const std::string&) override {if(failed) throw std::runtime_error("offline");return {};}
    bool ready() noexcept override{return !failed;}
};
void check(bool condition) {if(!condition) throw std::runtime_error("assertion failed");}
int main() {try {Repository repository;application::QueryService service(repository);
check(service.health().status==200);check(service.ready().status==200);
check(service.metrics({"host",1,100,100}).status==200);check(repository.calls==1);
check(service.metrics({std::string("x\0y",3),1,100,100}).status==400);check(repository.calls==1);
// Opaque v1 host IDs may contain quotes/slashes/UTF-8. Safety comes from SQL parameters.
check(!domain::validate_host_id("x' OR 1=1"));check(!domain::validate_host_id("máy/host"));
check(domain::validate_host_id(std::string("\xC0\xAF",2)).has_value());
bool corrupt=false;try{domain::rows_to_json({{"h","m",1,2,std::numeric_limits<double>::quiet_NaN(),1,2}});}catch(...){corrupt=true;}check(corrupt);
check(service.metrics({"host",1,domain::kMaxRangeMs+2,100}).status==400);
check(service.metrics({"host",2,1,100}).status==400);check(service.metrics({"host",1,2,1001}).status==400);
check(service.metrics({"host",9223372036854775806LL,9223372036854775807LL,1}).status==400);
check(service.latest("host").body=="{\"schema_version\":1,\"data\":[]}");
check(domain::json_escape("\"\n")=="\\\"\\u000a");repository.failed=true;
check(service.ready().status==503);check(service.metrics({"host",1,2,1}).status==503);check(service.latest("host").status==503);
std::cout<<"query assertions PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
