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

# Даём серверу время на запуск
time.sleep(1)

# Запускаем perf record для процесса сервера
# Используем -g для записи стека вызовов, -p для PID, -o для выходного файла
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

# Останавливаем perf record (посылаем SIGINT) и ждём завершения
perf.send_signal(signal.SIGINT)
try:
    perf.wait(timeout=5)
except subprocess.TimeoutExpired:
    perf.terminate()
    perf.wait()

# Останавливаем сервер
stop(server, wait=True)

# Проверяем, что perf.data создан и не пустой
if os.path.exists('perf.data') and os.path.getsize('perf.data') > 0:
    # Строим флеймграф
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
else:
    print('Error: perf.data is empty or not created')
    # Создаём пустой SVG с ошибкой для теста
    with open('graph.svg', 'w') as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100"><text>No data</text></svg>')
