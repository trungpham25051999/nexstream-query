#include "nexstream/query/domain/query_model.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <stdexcept>
namespace nexstream::query::domain {
std::optional<std::string> validate_host_id(std::string_view v) {
    if(v.empty() || v.size()>128) return "invalid host_id";
    for(size_t i=0;i<v.size();){
        const auto c=static_cast<unsigned char>(v[i++]);
        if(c<32||c==127)return "invalid host_id";
        if(c<128)continue;
        unsigned continuation=0,code=0,minimum=0;
        if(c>=0xC2&&c<=0xDF){continuation=1;code=c&31;minimum=128;}
        else if(c>=0xE0&&c<=0xEF){continuation=2;code=c&15;minimum=2048;}
        else if(c>=0xF0&&c<=0xF4){continuation=3;code=c&7;minimum=65536;}
        else return "invalid UTF-8 host_id";
        if(i+continuation>v.size())return "invalid UTF-8 host_id";
        for(unsigned j=0;j<continuation;++j){auto next=static_cast<unsigned char>(v[i++]);if((next&192)!=128)return "invalid UTF-8 host_id";code=(code<<6)|(next&63);}
        if(code<minimum||code>0x10FFFF||(code>=0xD800&&code<=0xDFFF))return "invalid UTF-8 host_id";
    }
    return {};
}
std::optional<std::string> validate_range(const RangeRequest& r) {
    if(auto e=validate_host_id(r.host_id)) return e;
    // Public API policy: positive Unix milliseconds through 2100-01-01, avoiding storage overflow.
    if(r.from_ms<=0 || r.to_ms>4'102'444'800'000LL || r.to_ms<r.from_ms || r.to_ms-r.from_ms>kMaxRangeMs || r.limit==0 || r.limit>kMaxRows) return "invalid range or limit";
    return {};
}
std::string json_escape(std::string_view v) {
    std::ostringstream s;
    for(unsigned char c:v) { if(c=='"'||c=='\\') s<<'\\'<<c; else if(c<32) s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c); else s<<c; }
    return s.str();
}
std::string rows_to_json(const std::vector<MetricRow>& rows) {
    std::ostringstream s; s<<std::setprecision(17)<<"{\"schema_version\":1,\"data\":["; bool first=true;
    for(const auto& r:rows) {
        if(!std::isfinite(r.cpu_usage_percent)||r.cpu_usage_percent<0||r.cpu_usage_percent>100||r.memory_total_bytes==0||r.memory_used_bytes>r.memory_total_bytes)throw std::runtime_error("invalid stored metric");
        if(!first) s<<',';
        first=false;
        s<<"{\"host_id\":\""<<json_escape(r.host_id)<<"\",\"message_id\":\""<<json_escape(r.message_id)<<"\",\"sampled_at_ms\":"<<r.sampled_at_ms<<",\"ingested_at_ms\":"<<r.ingested_at_ms<<",\"cpu_usage_percent\":"<<r.cpu_usage_percent<<",\"memory_used_bytes\":"<<r.memory_used_bytes<<",\"memory_total_bytes\":"<<r.memory_total_bytes<<'}';
    }
    s<<"]}"; return s.str();
}
}
