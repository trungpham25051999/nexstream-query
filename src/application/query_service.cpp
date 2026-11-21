#include "nexstream/query/application/query_service.hpp"
namespace nexstream::query::application {
Response QueryService::health() const { return {200,"{\"status\":\"live\"}"}; }
Response QueryService::ready() const { return repository_.ready()?Response{200,"{\"status\":\"ready\"}"}:Response{503,"{\"error\":\"storage unavailable\"}"}; }
Response QueryService::metrics(const domain::RangeRequest& r) {
    if(domain::validate_range(r)) return {400,"{\"error\":\"invalid range, host or limit\"}"};
    try { return {200,domain::rows_to_json(repository_.range(r))}; } catch(...) { return {503,"{\"error\":\"storage unavailable\"}"}; }
}
Response QueryService::latest(const std::string& host) {
    if(domain::validate_host_id(host)) return {400,"{\"error\":\"invalid host\"}"};
    try { auto row=repository_.latest(host); return {200,domain::rows_to_json(row?std::vector{*row}:std::vector<domain::MetricRow>{})}; } catch(...) { return {503,"{\"error\":\"storage unavailable\"}"}; }
}
}
