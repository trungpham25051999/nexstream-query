#pragma once

#include "nexstream/query/application/query_service.hpp"

#include <chrono>
#include <string>

namespace nexstream::query::infrastructure {

class ClickHouseClient final : public application::IMetricsRepository {
  public:
    ClickHouseClient(std::string base_url, std::chrono::milliseconds connect_timeout,
                     std::chrono::milliseconds query_timeout);
    std::vector<domain::MetricRow> range(const domain::RangeRequest& request) override;
    std::optional<domain::MetricRow> latest(const std::string& host_id) override;
    bool ready() noexcept override;

  private:
    std::string execute(std::string sql, const std::vector<std::pair<std::string, std::string>>& parameters);
    std::string base_url_;
    std::string username_, password_;
    std::chrono::milliseconds connect_timeout_;
    std::chrono::milliseconds query_timeout_;
};

}  // namespace nexstream::query::infrastructure
