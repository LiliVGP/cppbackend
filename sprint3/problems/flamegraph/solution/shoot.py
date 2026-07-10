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

# Запускаем сервер через perf record с правильными флагами
# -g (call-graph) и -F 99 для частоты
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-F', '99', '-g', '--'] + shlex.split(server_cmd),
    stderr=subprocess.DEVNULL
)

# Даём серверу время запуститься
time.sleep(2)

# Обстреливаем
make_shots()

# Даём perf время собрать данные
time.sleep(1)

# Корректно завершаем perf record
perf.send_signal(signal.SIGINT)
perf.wait()

# Проверяем perf.data
if not os.path.exists('perf.data') or os.path.getsize('perf.data') == 0:
    print("ERROR: perf.data is empty or does not exist!")
    sys.exit(1)

# Строим флеймграф с явными путями
flamegraph_dir = './FlameGraph'

# Запускаем perf script и проверяем, что есть данные
perf_script = subprocess.Popen(
    ['perf', 'script', '-i', 'perf.data'],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE
)

# Сначала проверим, что perf script выдаёт хоть что-то
stdout, stderr = perf_script.communicate()
if not stdout:
    print("ERROR: perf script produced no output!")
    sys.exit(1)

# Теперь строим флеймграф
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

perf_script.wait()
stackcollapse.wait()
flamegraph.wait()

# Проверяем результат
if os.path.exists('graph.svg'):
    with open('graph.svg', 'r') as f:
        content = f.read()
        if 'RequestHandler' not in content:
            # Проверим, что вообще есть в graph.svg
            print("WARNING: graph.svg does not contain RequestHandler")
            print(f"Content preview: {content[:200]}")
        else:
            print("SUCCESS: graph.svg contains RequestHandler functions")
else:
    print("ERROR: graph.svg was not created!")
    sys.exit(1)

print('Job done')
