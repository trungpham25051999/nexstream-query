# nexstream-query — processing guide

> Sprint 07 local implementation executed; see README and tests/results for limits/evidence.

## Implemented hardening workflow

Private acceptor -> 32-entry bounded queue -> four fixed workers -> bounded header/path/timestamp
validation -> typed ClickHouse parameters on per-request CURL handle -> validated rows -> JSON response.
A signal-wait thread stops acceptance and joins workers. Real concurrent/slow-client and sanitizer
evidence: `tests/results/2026-11-22-sprint-07-hardening/`.

## Test/evidence to maintain after implementation

- Contract version and related ADR:
- Input and output examples:
- Success and error tests:
- Exact command/run timestamp:
- Observed metrics/logs (or NOT RUN):
- Known limitations:
- Next action:

See `nexstream-deploy/docs/` for end-to-end behavior.
