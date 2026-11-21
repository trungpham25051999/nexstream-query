# Sprint 07 test inventory

| ID | Case | Final status | Evidence |
|---|---|---|---|
| Q01 | Host/range/limit validation | PASS | final-build-matrix.log |
| Q02 | JSON escaping/empty result schema | PASS | final-build-matrix.log |
| Q03 | Repository exception maps to 503 | PASS | final-build-matrix.log |
| Q04 | Real HTTP health/readiness/range/latest | PASS | http-integration-final.log |
| Q05 | Real timestamp/UInt64 mapping and duplicate IDs | PASS | http-integration-final.log |
| Q06 | ClickHouse unavailable and restart recovery | PASS | deploy report command transcript, rerun HTTP |
| Q07 | Incomplete slow headers/oversized headers | PASS | actual socket check: close at 3 s / immediate 8 KiB bound |

7 PASS, 0 FAIL, 0 BLOCKED, 0 NOT RUN. Unit CTest executable passed in three build configurations.

## Sprint 07 hardening

| ID | Case | Status | Actual evidence in results/2026-11-22-sprint-07-hardening/ |
|---|---|---|---|
| H-Q01 | Escaped UTF-8/quote/slash/percent host and parameterized injection isolation | PASS | http-final.log |
| H-Q02 | Invalid timestamps, overflow, repeated args, bad percent/UTF-8/control bytes | PASS | http-final.log |
| H-Q03 | Slow/incomplete/oversized headers and slow response-reader partial writes | PASS | http-final.log |
| H-Q04 | 200 real ClickHouse queries / 16 concurrent HTTP clients, measured percentiles | PASS | http-final.log, latency.json |
| H-Q05 | 64-client admission burst, six observed threads / forty FDs, recovery | PASS | http-final.log |
| H-Q06 | Corrupt stored CPU fails closed, never fabricated zero | PASS | http-final.log, build-matrix-final.log |
| H-Q07 | Debug/Release/ASan+UBSan unit suites | PASS | build-matrix-final.log |
| H-Q08 | Real concurrent/slow HTTP under ASan; clean signal shutdown | PASS | asan-http.log, asan-server.log |
| H-Q09 | Legacy range/latest/schema integration after additive host acceptance | PASS | legacy-integration.log |

9 PASS, 0 FAIL, 0 BLOCKED, 0 NOT RUN in the required hardening catalog.
Supplementary H-QX1: ThreadSanitizer is NOT RUN (separate from requested ASan/UBSan matrix).
The old quote-host 400 expectation was replaced because printable escaped IDs are intentionally now
accepted. Real SQL-parameter isolation still returns an empty result for a nonexistent injection-like
literal, and invalid control-byte IDs remain 400; this is an API policy change, not weakened security.
