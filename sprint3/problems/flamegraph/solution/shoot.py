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

# Запускаем сервер в фоне
server = run(server_cmd)
time.sleep(1)

# Запускаем perf record с захватом символов
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid), '-F', '99', '-g', '--call-graph', 'dwarf'],
    stderr=subprocess.DEVNULL
)

time.sleep(1)

# Обстреливаем
make_shots()

time.sleep(1)

# Останавливаем perf
perf.send_signal(signal.SIGINT)
perf.wait()

# Останавливаем сервер
stop(server)

# Проверяем perf.data
if not os.path.exists('perf.data') or os.path.getsize('perf.data') == 0:
    print("ERROR: perf.data is empty or does not exist!")
    sys.exit(1)

# Строим флеймграф с демангляцией
flamegraph_dir = './FlameGraph'

perf_script = subprocess.Popen(
    ['perf', 'script', '-i', 'perf.data'],
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

# Добавляем c++filt для демангляции имён
cxxfilt = subprocess.Popen(
    ['c++filt'],
    stdin=perf_script.stdout,
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

stackcollapse = subprocess.Popen(
    [f'{flamegraph_dir}/stackcollapse-perf.pl'],
    stdin=cxxfilt.stdout,
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
cxxfilt.wait()
stackcollapse.wait()
flamegraph.wait()

# Проверяем graph.svg
if os.path.exists('graph.svg'):
    with open('graph.svg', 'r') as f:
        content = f.read()
        # Проверяем разные варианты имени
        if ('RequestHandler' in content or 
            'http_handler::RequestHandler' in content or
            '_ZN12http_handler14RequestHandler' in content):
            print("SUCCESS: graph.svg contains RequestHandler functions")
        else:
            print(f"WARNING: graph.svg does not contain RequestHandler. Content preview: {content[:500]}")
            # Выведем первые несколько строк для отладки
            lines = content.split('\n')
            for i, line in enumerate(lines[:20]):
                print(f"{i}: {line}")
else:
    print("ERROR: graph.svg was not created!")
    sys.exit(1)

print('Job done')
