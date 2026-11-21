"""Real HTTP/ClickHouse integration. Requires the documented s07 fixture."""
import json
import urllib.request
import urllib.error

def get(path):
    try:
        with urllib.request.urlopen('http://127.0.0.1:8080' + path, timeout=5) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        return error.code, json.load(error)

assert get('/health')[0] == 200
assert get('/health/ready')[0] == 200
status, latest = get('/v1/hosts/s07-host/latest')
assert status == 200 and latest['data'][0]['message_id'] == 's07-m2'
assert latest['data'][0]['memory_total_bytes'] == 100
assert latest['data'][0]['sampled_at_ms'] == 1793650001000
status, rows = get('/v1/hosts/s07-host/metrics?from=1793650000000&to=1793650001000')
assert status == 200 and len(rows['data']) == 2
assert get('/v1/hosts/s07-host/metrics?from=2&to=1')[0] == 400
assert get('/v1/hosts/s07-host/metrics?from=1&to=2&limit=1001')[0] == 400
assert get('/v1/hosts/s07-host/metrics?from=1&from=2&to=3')[0] == 400
status, opaque = get('/v1/hosts/bad%27/latest')
assert status == 200 and opaque['data'] == []
assert get('/v1/hosts/bad%00/latest')[0] == 400
print('PASS: real HTTP health/readiness/latest/range/mapping/dedup/validation')
