#pragma once

#include "nexstream/query/domain/query_model.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace nexstream::query::application {

// HttpServer calls this port concurrently on at most four workers. Implementations must be thread-safe.
class IMetricsRepository {
  public:
    virtual ~IMetricsRepository() = default;
    virtual std::vector<domain::MetricRow> range(const domain::RangeRequest& request) = 0;
    virtual std::optional<domain::MetricRow> latest(const std::string& host_id) = 0;
    virtual bool ready() noexcept = 0;
};

struct Response {
    int status{};
    std::string body;
};

class QueryService final {
  public:
    explicit QueryService(IMetricsRepository& repository) : repository_(repository) {}
    Response health() const;
    Response ready() const;
    Response metrics(const domain::RangeRequest& request);
    Response latest(const std::string& host_id);

  private:
    IMetricsRepository& repository_;
};

}  // namespace nexstream::query::application
