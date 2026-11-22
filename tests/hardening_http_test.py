"""Real private Query/ClickHouse fixture, bounded concurrent and adversarial clients."""
import concurrent.futures
import json
import math
import os
import pathlib
import socket
import statistics
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'tests/results/2026-11-22-sprint-07-hardening'
def get(path):
    start=time.perf_counter()
    try:
        with urllib.request.urlopen('http://127.0.0.1:8080'+path,timeout=8) as response:
            result=(response.status,json.load(response))
    except urllib.error.HTTPError as error:
        result=(error.code,json.load(error) if error.headers.get('Content-Length')!='0' else {})
    return (*result,(time.perf_counter()-start)*1000)
def sql(query):
    req=urllib.request.Request('http://127.0.0.1:28123/?user=hardening_fixture',data=query.encode())
    with urllib.request.urlopen(req,timeout=15) as response:return response.read().decode()

host="máy/'% host";stamp=time.strftime('%Y-%m-%d %H:%M:%S',time.gmtime())+'.000'
record=dict(schema_version=1,host_id=host,message_id='escaped-id',sampled_at=stamp,ingested_at=stamp,cpu_usage_percent=50,memory_used_bytes=50,memory_total_bytes=100)
sql('INSERT INTO nexstream.telemetry_raw FORMAT JSONEachRow\n'+json.dumps(record))
status,data,_=get('/v1/hosts/'+urllib.parse.quote(host,safe='')+'/latest')
assert status==200 and data['data'][0]['host_id']==host
status,data,_=get('/v1/hosts/'+urllib.parse.quote("x' OR 1=1 --",safe='')+'/latest')
assert status==200 and data['data']==[]
for raw in ['%','%ZZ','%00','%C0%AF','%ED%A0%80']:
    assert get('/v1/hosts/'+raw+'/latest')[0]==400
for query in ['from=-1&to=1','from=x&to=2','from=2&to=1','from=1&to=2&limit=1001',
              'from=9223372036854775806&to=9223372036854775807','from=9223372036854775808&to=9223372036854775809',
              'from=1&to=2&from=3']:
    assert get('/v1/hosts/test/metrics?'+query)[0]==400
print('PASS escaped UTF-8/quote/slash/percent identity, parameterized injection isolation, malformed path and timestamp bounds')
corrupt_host='corrupt-'+str(time.time_ns())
sql(f"INSERT INTO nexstream.telemetry_raw SELECT 1,'{corrupt_host}','invalid',now64(3),now64(3),toFloat64('nan'),toUInt64(1),toUInt64(2)")
assert get('/v1/hosts/'+corrupt_host+'/latest')[0]==503
print('PASS corrupt stored metric fails closed rather than fabricating zero CPU')

slow=[socket.create_connection(('127.0.0.1',8080)) for _ in range(2)]
for client in slow:client.settimeout(5);client.sendall(b'GET /health HTTP/1.1\r\n')
status,_,elapsed=get('/health');assert status==200 and elapsed<1000
for client in slow:assert client.recv(1024)==b'';client.close()
oversized=socket.create_connection(('127.0.0.1',8080));oversized.settimeout(5)
try:
    oversized.sendall(b'GET /health HTTP/1.1\r\nX: '+b'x'*10000)
    assert oversized.recv(1024)==b''
except ConnectionResetError:pass
finally:oversized.close()
print('PASS slow clients do not monopolize all workers; incomplete/oversized requests close within bounds')

bench=json.loads((ROOT.parent/'nexstream-deploy/tests/results/2026-11-22-sprint-07-hardening/dedup-measurements.json').read_text())
request='/v1/hosts/'+bench['fixture_host']+'/latest'
for _ in range(10):assert get(request)[0]==200
with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:results=list(pool.map(lambda _:get(request),range(200)))
assert all(status==200 and len(data['data'])==1 for status,data,_ in results)
samples=sorted(r[2] for r in results)
measurement={'endpoint':'latest','requests':200,'concurrency':16,'warmup':10,'connection_reuse':False,
             'build':os.environ.get('NEXSTREAM_QUERY_BUILD','Debug'),
             'fixture_raw_rows':100000,'fixture_logical_ids':50000,'p50_ms':statistics.median(samples),
             'p95_ms':samples[math.ceil(.95*len(samples))-1],'p99_ms':samples[math.ceil(.99*len(samples))-1],
             'samples_ms':samples,'machine_log':'nexstream-deploy/tests/results/2026-11-22-sprint-07-hardening/environment.log'}
OUT.mkdir(parents=True,exist_ok=True);(OUT/('latency-asan.json' if measurement['build']=='ASan' else 'latency.json')).write_text(json.dumps(measurement,indent=2))
print('PASS 200 real ClickHouse-backed HTTP requests with 16 concurrent clients')
print('MEASURED local HTTP:',json.dumps({k:v for k,v in measurement.items() if k!='samples_ms'}))

# Force partial response writes/backpressure rather than testing only slow request headers.
bounds=json.loads(sql("SELECT min(toUnixTimestamp64Milli(sampled_at)) AS lo,max(toUnixTimestamp64Milli(sampled_at)) AS hi FROM nexstream.telemetry_raw WHERE host_id='"+bench['fixture_host']+"' FORMAT JSONEachRow"))
large='/v1/hosts/'+bench['fixture_host']+'/metrics?from='+str(bounds['lo'])+'&to='+str(bounds['hi'])+'&limit=1000'
reader=socket.socket();reader.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,1024);reader.settimeout(12)
reader.connect(('127.0.0.1',8080));reader.sendall(('GET '+large+' HTTP/1.1\r\nHost: localhost\r\n\r\n').encode())
assert get('/health')[0]==200
time.sleep(6.5)
received=b''
while True:
    part=reader.recv(65536)
    if not part:break
    received+=part
reader.close();header,body=received.split(b'\r\n\r\n',1)
expected=int(next(line.split(b':',1)[1] for line in header.split(b'\r\n') if line.lower().startswith(b'content-length:')))
assert header.startswith(b'HTTP/1.1 200') and len(body)<expected
print('PASS slow response reader: partial writes bounded by send deadline, received_bytes=',len(body),'declared_bytes=',expected)

def hold(_):
    try:
        client=socket.create_connection(('127.0.0.1',8080),timeout=2);client.sendall(b'GET /health HTTP/1.1\r\n');return client
    except OSError:return None
with concurrent.futures.ThreadPoolExecutor(max_workers=64) as pool:clients=list(pool.map(hold,range(64)))
if os.environ.get('NEXSTREAM_QUERY_PID'):
    proc=pathlib.Path('/proc')/os.environ['NEXSTREAM_QUERY_PID']
    threads=int(next(line.split()[1] for line in (proc/'status').read_text().splitlines() if line.startswith('Threads:')))
    descriptors=len(list((proc/'fd').iterdir()))
    assert threads<=6 and descriptors<=42
    print('OBSERVED bounded Query admission:',json.dumps({'threads':threads,'open_descriptors':descriptors,'worker_threads':4,'pending_limit':32}))
for client in clients:
    if client:client.close()
end=time.monotonic()+5
while time.monotonic()<end:
    try:
        if get('/health')[0]==200:break
    except OSError:pass
else:raise AssertionError('server did not recover after bounded admission overload')
print('PASS 64-client incomplete-header admission burst and subsequent recovery')
