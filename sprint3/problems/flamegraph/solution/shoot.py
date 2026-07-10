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

# Запускаем сервер
server = run(server_cmd)

# Даём серверу время запуститься
time.sleep(1)

# Запускаем perf record с флагом -p для PID сервера и -o для явного указания файла
# Добавляем -F 99 для частоты семплирования и --call-graph dwarf для захвата стека
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid), '-F', '99', '--call-graph', 'dwarf'],
    stderr=subprocess.DEVNULL
)

# Даём perf время начать запись
time.sleep(1)

# Обстреливаем сервер
make_shots()

# Даём perf время собрать данные после обстрела
time.sleep(1)

# Корректно останавливаем perf record
perf.send_signal(signal.SIGINT)
perf.wait()

# Останавливаем сервер
stop(server)

# Проверяем, что perf.data не пустой
if not os.path.exists('perf.data') or os.path.getsize('perf.data') == 0:
    print("ERROR: perf.data is empty or does not exist!")
    sys.exit(1)

# Строим флеймграф
# Используем явные пути к скриптам
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

# Проверяем, что graph.svg создан и содержит нужные функции
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
