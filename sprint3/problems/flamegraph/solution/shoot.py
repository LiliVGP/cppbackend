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

# Запускаем сервер в фоновом процессе
print(f"Starting server: {server_cmd}")
server = run(server_cmd)

# Даём серверу время на запуск - побольше
time.sleep(3)

# Проверяем, что сервер запустился
if server.poll() is not None:
    print("Server terminated! Retrying...")
    # Перезапускаем с абсолютным путём
    server_cmd_abs = os.path.abspath(server_cmd)
    print(f"Retrying with: {server_cmd_abs}")
    server = run(server_cmd_abs)
    time.sleep(3)

# Запускаем perf record для процесса сервера
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

# Останавливаем perf record
perf.send_signal(signal.SIGINT)
try:
    perf.wait(timeout=5)
except subprocess.TimeoutExpired:
    perf.terminate()
    perf.wait()

# Останавливаем сервер
stop(server, wait=True)

# Строим флеймграф
# perf script | stackcollapse-perf.pl | flamegraph.pl > graph.svg
if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
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

    flamegraph = subprocess.Popen(
        ['./FlameGraph/flamegraph.pl'],
        stdin=stackcollapse.stdout,
        stdout=open('graph.svg', 'w'),
        stderr=subprocess.DEVNULL
    )

    perf_script.wait()
    stackcollapse.wait()
    flamegraph.wait()

print('Job done')
