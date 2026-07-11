import argparse
import subprocess
import time
import random
import shlex
import os
import signal
import sys
import tempfile
import json

RANDOM_LIMIT = 1000
SEED = 123456789
random.seed(SEED)

AMMUNITION = [
    'localhost:8080/api/v1/maps/map1',
    'localhost:8080/api/v1/maps'
]

SHOOT_COUNT = 500
COOLDOWN = 0.02


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


# Получаем команду для запуска сервера
server_cmd = start_server()

# Создаём временный конфигурационный файл
config_data = {
    "maps": [
        {
            "id": "map1",
            "name": "Map 1"
        }
    ]
}

with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
    json.dump(config_data, f)
    config_path = f.name

print(f"Created temporary config: {config_path}")

# Разбиваем команду на части (может содержать путь к серверу и другие аргументы)
cmd_parts = shlex.split(server_cmd)
server_exe = cmd_parts[0]

# Запускаем сервер с конфигурационным файлом
server = subprocess.Popen(
    [server_exe, config_path],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.PIPE,
    text=True
)

print(f"Server PID: {server.pid}")
time.sleep(2)

# Проверяем, что сервер жив
if server.poll() is not None:
    print(f"ERROR: Server died! Return code: {server.returncode}")
    stderr = server.stderr.read()
    if stderr:
        print(f"Server stderr: {stderr}")
    os.unlink(config_path)
    sys.exit(1)

# Запускаем perf record
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-p', str(server.pid), '-F', '999', '-g', '--', 'sleep', '10'],
    stderr=subprocess.DEVNULL
)

time.sleep(1)

# Обстреливаем
make_shots()

time.sleep(2)

# Останавливаем perf
perf.send_signal(signal.SIGINT)
perf.wait()

# Останавливаем сервер
stop(server)
os.unlink(config_path)

# Проверяем perf.data
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
        if 'http_handler::RequestHandler' in content:
            print("SUCCESS: graph.svg contains RequestHandler functions")
        else:
            print("WARNING: RequestHandler not found")
else:
    print("ERROR: graph.svg was not created!")
    sys.exit(1)

print('Job done')
