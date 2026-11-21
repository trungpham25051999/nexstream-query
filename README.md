# nexstream-query

C++20 read-only ClickHouse HTTP API, Sprint 07 development implementation.

Build: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ctest --test-dir build --output-on-failure`.
Requires CMake 3.20+, GCC 12/C++20 and libcurl development headers (tested 7.88.1).
Use `-DNEXSTREAM_ENABLE_SANITIZERS=ON` for ASan/UBSan.

Run `./build/nexstream-query http://127.0.0.1:8123`. Listens on **127.0.0.1:8080**.
Optional environment: `NEXSTREAM_CH_USER`, `NEXSTREAM_CH_PASSWORD_FILE` (runtime secret).

- `GET /health` and `/health/live`: process liveness.
- `GET /health/ready`: ClickHouse connectivity; 503 when unavailable.
- `GET /v1/hosts/{host_id}/metrics?from=1793650000000&to=1793650001000&limit=100`.
- `GET /v1/hosts/{host_id}/latest`.

Timestamps are positive Unix milliseconds through 2100-01-01, inclusive bounds, ordered range <=7 days.
Host IDs are printable UTF-8, 1–128 bytes; percent-encode path segments, including quotes/slashes/percent.
Decoding occurs once. Malformed escapes, control bytes and invalid UTF-8 are rejected.
This expands the former ASCII whitelist without modifying Protobuf v1. Default limit 100, maximum 1000.
Unknown/repeated query arguments are rejected. Empty results return 200 with an empty array.
JSON response: `schema_version=1`, `data` array with `host_id`, `message_id`, `sampled_at_ms`,
`ingested_at_ms`, `cpu_usage_percent`, `memory_used_bytes`, `memory_total_bytes`.
Errors return `{"error":"..."}` with 400/404/503.

ClickHouse typed placeholders and URL-encoded values prevent SQL interpolation.
Connect timeout 500 ms, total query deadline 2500 ms, SQL execution 2 s, result 2 MiB/1000 rows,
SQL memory 64 MiB. HTTP headers <=8192 bytes; four fixed workers and a 32-entry pending queue.
Read deadline is three seconds from acceptance; overall send deadline is acceptance plus six seconds.
Expired queued clients are closed on dispatch. Full admission returns nonblocking 503 or closes.
Accepted sockets request 16 KiB receive/send buffers (Linux reports doubled values plus overhead).
Each ClickHouse request owns its CURL handle; repository implementations must support concurrent calls.
SIGINT/SIGTERM are consumed by a sigwait thread and drain the bounded active workers safely.
Corrupt stored metrics return 503 instead of fabricating zero CPU. API remains loopback/private only.
Read-time identity deduplication is defined in deploy SQL; Kafka ACK remains separate from visibility.

Run `python3 tests/integration_test.py` with the documented local S07 fixture.
See `tests/TEST_CASES.md`, `tests/results/2026-11-21-sprint-07/` and the deploy Sprint 07 report.
Hardening: `python3 tests/hardening_http_test.py` against the dedicated 28123 ClickHouse fixture;
see `tests/results/2026-11-22-sprint-07-hardening/` for actual latency samples and bounded-client tests.
