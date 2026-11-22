# Actual hardening evidence

Required H-Q01..H-Q09: 9 PASS. Supplementary H-QX1 ThreadSanitizer: NOT RUN.
`build-matrix-final.log`: Debug/Release/Debug+ASan/UBSan unit suites.
`http-final.log`: actual escaped/invalid/corrupt/slow/large/concurrent/admission behavior, including
slow-reader partial response writes. `latency.json` contains the final Debug samples and workload.
`asan-http.log`, `latency-asan.json`: separate actual HTTP sanitizer run.
`asan-server.log` is empty: the server emitted no sanitizer diagnostics; SIGINT exit 0 was observed.
`legacy-integration.log`: actual existing schema/range/latest integration after additive host acceptance.
Earlier `http.log` retains the missing-measurement-fixture failure; final dependency order succeeded.
See deploy hardening report for machine metadata and interpretation limits.
