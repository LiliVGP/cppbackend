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

# Запускаем сервер через perf record с флагом --call-graph dwarf
# Это запустит perf record, который выполнит сервер
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-F', '99', '--call-graph', 'dwarf', '--'] + shlex.split(server_cmd),
    stderr=subprocess.DEVNULL
)

# Даём серверу время запуститься и perf начать запись
time.sleep(2)

# Обстреливаем сервер
make_shots()

# Даём perf время собрать данные после обстрела
time.sleep(1)

# Корректно останавливаем perf record (посылаем SIGINT, а не SIGTERM)
perf.send_signal(signal.SIGINT)
perf.wait()

# Проверяем, что perf.data не пустой
if not os.path.exists('perf.data') or os.path.getsize('perf.data') == 0:
    print("ERROR: perf.data is empty or does not exist!")
    sys.exit(1)

# Строим флеймграф
flamegraph_dir = './FlameGraph'

perf_script = subprocess.Popen(
    ['perf', 'script', '-i', 'perf.data'],
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

stackcollapse = subprocess.Popen(
    [f'{flamegraph_dir}/stackcollapse-perf.pl'],
    stdin=perf_script.stdout,
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

flamegraph = subprocess.Popen(
    [f'{flamegraph_dir}/flamegraph.pl'],
    stdin=stackcollapse.stdout,
    stdout=open('graph.svg', 'w'),
    stderr=subprocess.DEVNULL
)

# Ждём завершения всех процессов
perf_script.wait()
stackcollapse.wait()
flamegraph.wait()

# Проверяем, что graph.svg содержит RequestHandler
if os.path.exists('graph.svg'):
    with open('graph.svg', 'r') as f:
        content = f.read()
        if 'RequestHandler' not in content:
            print("WARNING: graph.svg does not contain RequestHandler functions")
        else:
            print("SUCCESS: graph.svg created successfully")
else:
    print("ERROR: graph.svg was not created!")
    sys.exit(1)

print('Job done')
