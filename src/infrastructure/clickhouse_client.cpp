#include "nexstream/query/infrastructure/clickhouse_client.hpp"
#include <curl/curl.h>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
namespace nexstream::query::infrastructure {
namespace {
size_t append(char* p,size_t n,size_t m,void* context) {
    auto& out=*static_cast<std::string*>(context);
    if(m && n> (2*1024*1024-out.size())/m) return 0;
    try { out.append(p,n*m); return n*m; } catch(...) {return 0;}
}
std::vector<domain::MetricRow> parse(const std::string& text) {
    auto decode=[](const std::string& value) {
        if(value.empty()||value.size()>256||value.size()%2) throw std::runtime_error("invalid hex id");
        auto digit=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;throw std::runtime_error("hex");};
        std::string result;
        for(size_t i=0;i<value.size();i+=2)result+=static_cast<char>(digit(value[i])*16+digit(value[i+1]));
        return result;
    };
    std::istringstream in(text); std::vector<domain::MetricRow> rows; std::string line;
    while(std::getline(in,line)) {
        domain::MetricRow row;std::istringstream columns(line);
        if(!(columns>>row.host_id>>row.message_id>>row.sampled_at_ms>>row.ingested_at_ms>>row.cpu_usage_percent>>row.memory_used_bytes>>row.memory_total_bytes))throw std::runtime_error("invalid storage row");
        columns>>std::ws;if(!columns.eof())throw std::runtime_error("extra storage columns");
        row.host_id=decode(row.host_id);row.message_id=decode(row.message_id);
        if(rows.size()>=domain::kMaxRows) throw std::runtime_error("row limit");
        rows.push_back(row);
    }
    if(!in.eof()) throw std::runtime_error("invalid storage response");
    return rows;
}
const std::string projection="SELECT hex(host_id),hex(message_id),toUnixTimestamp64Milli(sampled_at),toUnixTimestamp64Milli(ingested_at),cpu_usage_percent,memory_used_bytes,memory_total_bytes FROM nexstream.telemetry_deduplicated ";
}
ClickHouseClient::ClickHouseClient(std::string url,std::chrono::milliseconds connect,std::chrono::milliseconds timeout):base_url_(std::move(url)),connect_timeout_(connect),query_timeout_(timeout) {
    static const int initialized=curl_global_init(CURL_GLOBAL_DEFAULT); if(initialized!=CURLE_OK) throw std::runtime_error("curl init");
    if(const char* user=std::getenv("NEXSTREAM_CH_USER"))username_=user;
    if(const char* file=std::getenv("NEXSTREAM_CH_PASSWORD_FILE")){std::ifstream in(file);if(!std::getline(in,password_)||password_.size()>4096)throw std::runtime_error("ClickHouse credential file");}
}
std::string ClickHouseClient::execute(std::string sql,const std::vector<std::pair<std::string,std::string>>& params) {
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> handle(curl_easy_init(),curl_easy_cleanup); if(!handle) throw std::runtime_error("curl handle");
    std::string url=base_url_+"/?max_execution_time=2&max_result_rows=1000&result_overflow_mode=throw&max_memory_usage=67108864";
    for(const auto& [key,value]:params) { std::unique_ptr<char,decltype(&curl_free)> escaped(curl_easy_escape(handle.get(),value.c_str(),static_cast<int>(value.size())),curl_free); if(!escaped) throw std::runtime_error("escape"); url+='&'+key+'='+escaped.get(); }
    std::string response;
    if(!username_.empty()){curl_easy_setopt(handle.get(),CURLOPT_USERNAME,username_.c_str());curl_easy_setopt(handle.get(),CURLOPT_PASSWORD,password_.c_str());}
    curl_easy_setopt(handle.get(),CURLOPT_URL,url.c_str()); curl_easy_setopt(handle.get(),CURLOPT_POSTFIELDS,sql.c_str());
    curl_easy_setopt(handle.get(),CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(connect_timeout_.count())); curl_easy_setopt(handle.get(),CURLOPT_TIMEOUT_MS,static_cast<long>(query_timeout_.count()));
    curl_easy_setopt(handle.get(),CURLOPT_NOSIGNAL,1L); curl_easy_setopt(handle.get(),CURLOPT_WRITEFUNCTION,append); curl_easy_setopt(handle.get(),CURLOPT_WRITEDATA,&response);
    auto result=curl_easy_perform(handle.get()); long status=0; curl_easy_getinfo(handle.get(),CURLINFO_RESPONSE_CODE,&status);
    if(result!=CURLE_OK||status!=200) throw std::runtime_error("ClickHouse unavailable");
    return response;
}
std::vector<domain::MetricRow> ClickHouseClient::range(const domain::RangeRequest& r) {
    if(domain::validate_range(r)) throw std::invalid_argument("range");
    return parse(execute(projection+"WHERE host_id={host:String} AND sampled_at>=fromUnixTimestamp64Milli({from:Int64}) AND sampled_at<=fromUnixTimestamp64Milli({to:Int64}) ORDER BY sampled_at,message_id LIMIT {limit:UInt32} FORMAT TabSeparated",{{"param_host",r.host_id},{"param_from",std::to_string(r.from_ms)},{"param_to",std::to_string(r.to_ms)},{"param_limit",std::to_string(r.limit)}}));
}
std::optional<domain::MetricRow> ClickHouseClient::latest(const std::string& host) {
    if(domain::validate_host_id(host)) throw std::invalid_argument("host");
    auto rows=parse(execute(projection+"WHERE host_id={host:String} ORDER BY sampled_at DESC,message_id DESC LIMIT 1 FORMAT TabSeparated",{{"param_host",host}})); return rows.empty()?std::nullopt:std::optional(rows.front());
}
bool ClickHouseClient::ready() noexcept {try {return execute("SELECT 1",{}).starts_with("1");} catch(...) {return false;}}
}
