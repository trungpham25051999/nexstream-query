#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nexstream::query::domain {

inline constexpr std::int64_t kMaxRangeMs = 7LL * 24 * 60 * 60 * 1000;
inline constexpr std::size_t kMaxRows = 1000;

struct MetricRow {
    std::string host_id;
    std::string message_id;
    std::int64_t sampled_at_ms{};
    std::int64_t ingested_at_ms{};
    double cpu_usage_percent{};
    std::uint64_t memory_used_bytes{};
    std::uint64_t memory_total_bytes{};
};

struct RangeRequest {
    std::string host_id;
    std::int64_t from_ms{};
    std::int64_t to_ms{};
    std::size_t limit{100};
};

std::optional<std::string> validate_host_id(std::string_view value);
std::optional<std::string> validate_range(const RangeRequest& request);
std::string json_escape(std::string_view value);
std::string rows_to_json(const std::vector<MetricRow>& rows);

}  // namespace nexstream::query::domain
