import argparse
import subprocess
import time
import random
import shlex
import os
import signal
import sys

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 100
COOLDOWN = 0.1


def start_server():
    parser = argparse.ArgumentParser()
    parser.add_argument('server', type=str)
    return parser.parse_args().server


def run(command, output=None):
    process = subprocess.Popen(shlex.split(command), stdout=output, stderr=subprocess.DEVNULL)
    return process


def stop(process, wait=False):
    if process.poll() is None and wait:
        process.wait()
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()


def shoot(ammo):
    try:
        hit = run('curl ' + ammo, output=subprocess.DEVNULL)
        time.sleep(COOLDOWN)
        stop(hit, wait=True)
    except:
        pass


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


server_cmd = start_server()

# Запускаем сервер
print(f"Starting server: {server_cmd}")
server = run(server_cmd)

# Даём серверу время на запуск
time.sleep(2)

# Проверяем, что сервер работает
if server.poll() is not None:
    print(f"Server terminated immediately with code {server.poll()}")
    exit(1)

print(f"Server running with PID: {server.pid}")

# Сначала запускаем perf record
print(f"Starting perf record for PID {server.pid}")
perf = subprocess.Popen(
    ['perf', 'record', '-g', '-o', 'perf.data', '-p', str(server.pid)],
    stderr=subprocess.DEVNULL
)

# Даём perf время на начало записи
time.sleep(1)

# Выполняем обстрел
make_shots()

# Даём время на завершение последних запросов
time.sleep(1)

# Останавливаем perf record (SIGINT)
print("Stopping perf...")
perf.send_signal(signal.SIGINT)
perf.wait()

# Останавливаем сервер
print("Stopping server...")
stop(server, wait=True)

# Проверяем perf.data
if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    print(f"perf.data created, size: {os.path.getsize('perf.data')} bytes")
    
    # Строим флеймграф
    print("Building flamegraph...")
    try:
        # perf script | stackcollapse-perf.pl | flamegraph.pl > graph.svg
        perf_script = subprocess.Popen(
            ['perf', 'script', '-i', 'perf.data'],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )

        stackcollapse = subprocess.Popen(
            ['./FlameGraph/stackcollapse-perf.pl'],
            stdin=perf_script.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL
        )

        with open('graph.svg', 'w') as f:
            flamegraph = subprocess.Popen(
                ['./FlameGraph/flamegraph.pl'],
                stdin=stackcollapse.stdout,
                stdout=f,
                stderr=subprocess.DEVNULL
            )

        perf_script.wait()
        stackcollapse.wait()
        flamegraph.wait()
        
        print("Flamegraph created successfully")
        
    except Exception as e:
        print(f"Error building flamegraph: {e}")
else:
    print("Error: perf.data is empty or not created")

print("Job done")
