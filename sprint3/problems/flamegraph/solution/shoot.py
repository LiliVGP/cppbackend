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
    process.terminate()


def shoot(ammo):
    hit = run('curl ' + ammo, output=subprocess.DEVNULL)
    time.sleep(COOLDOWN)
    stop(hit, wait=True)


def make_shots():
    for _ in range(SHOOT_COUNT):
        ammo_number = random.randrange(RANDOM_LIMIT) % len(AMMUNITION)
        shoot(AMMUNITION[ammo_number])
    print('Shooting complete')


server_cmd = start_server()

# Запускаем сервер в фоновом процессе
server = run(server_cmd)

# Небольшая пауза, чтобы сервер успел запуститься
time.sleep(0.5)

# Запускаем perf record для процесса сервера
# Используем -p PID для привязки к процессу сервера и -o для указания выходного файла
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid)],
    stderr=subprocess.DEVNULL
)

# Небольшая пауза, чтобы perf успел начать запись
time.sleep(0.5)

# Выполняем обстрел
make_shots()

# Останавливаем perf record (посылаем SIGINT)
perf.send_signal(signal.SIGINT)
perf.wait()

# Останавливаем сервер
stop(server)

# Строим флеймграф
# perf script | stackcollapse-perf.pl | flamegraph.pl > graph.svg
# Важно: используем -i для указания входного файла
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

# Ждём завершения всех процессов в пайпе
perf_script.wait()
stackcollapse.wait()
flamegraph.wait()

print('Job done')
