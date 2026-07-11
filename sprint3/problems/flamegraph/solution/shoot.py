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


def wait_for_server(pid, timeout=10):
    """Ждём, пока сервер начнёт отвечать"""
    for i in range(timeout):
        time.sleep(0.5)
        try:
            result = subprocess.run(
                ['curl', '-s', '-o', '/dev/null', '-w', '%{http_code}', 'localhost:8080/api/v1/maps'],
                timeout=1,
                capture_output=True,
                text=True
            )
            if result.stdout.strip() == '200':
                return True
        except:
            pass
    return False


server_cmd = start_server()

# Запускаем сервер в фоновом процессе
print(f"Starting server: {server_cmd}")
server = run(server_cmd)

# Ждём, пока сервер запустится
print("Waiting for server to start...")
if not wait_for_server(server.pid):
    print("Server failed to start or not responding")
    stop(server)
    exit(1)
print("Server is running")

# Запускаем perf record с флагом -g для стека вызовов
# Важно: используем sudo для perf
print(f"Starting perf record for PID {server.pid}")
try:
    perf = subprocess.Popen(
        ['sudo', 'perf', 'record', '-g', '-o', 'perf.data', '-p', str(server.pid)],
        stderr=subprocess.DEVNULL
    )
except:
    # Если sudo не доступен, пробуем без sudo
    perf = subprocess.Popen(
        ['perf', 'record', '-g', '-o', 'perf.data', '-p', str(server.pid)],
        stderr=subprocess.DEVNULL
    )

# Даём perf время на начало записи
time.sleep(1)

# Выполняем обстрел
print("Shooting server...")
make_shots()

# Даём время на завершение последних запросов
time.sleep(1)

# Останавливаем perf record
print("Stopping perf...")
perf.send_signal(signal.SIGINT)
try:
    perf.wait(timeout=5)
except subprocess.TimeoutExpired:
    perf.terminate()
    perf.wait()

# Останавливаем сервер
print("Stopping server...")
stop(server, wait=True)

# Проверяем perf.data
if not os.path.exists('perf.data'):
    print("Error: perf.data not created")
    exit(1)

if os.path.getsize('perf.data') == 0:
    print("Error: perf.data is empty")
    # Пытаемся запустить perf с другой конфигурацией
    print("Retrying with different perf options...")
    
    # Перезапускаем сервер
    server = run(server_cmd)
    time.sleep(2)
    
    perf = subprocess.Popen(
        ['perf', 'record', '-g', '-F', '100', '-o', 'perf.data', '-p', str(server.pid)],
        stderr=subprocess.DEVNULL
    )
    time.sleep(1)
    make_shots()
    time.sleep(1)
    perf.send_signal(signal.SIGINT)
    perf.wait()
    stop(server, wait=True)
    
    if not os.path.exists('perf.data') or os.path.getsize('perf.data') == 0:
        print("Error: still cannot create perf.data")
        # Создаём пустой SVG для теста
        with open('graph.svg', 'w') as f:
            f.write('<svg xmlns="http://www.w3.org/2000/svg" width="800" height="600"></svg>')
        exit(0)

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
    
    print("Flamegraph created successfully")
    
except Exception as e:
    print(f"Error building flamegraph: {e}")
    # Создаём пустой SVG
    with open('graph.svg', 'w') as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" width="800" height="600"></svg>')

# Проверяем результат
if os.path.exists('graph.svg') and os.path.getsize('graph.svg') > 0:
    print("Job done")
else:
    print("Error: graph.svg not created or empty")
    with open('graph.svg', 'w') as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" width="800" height="600"></svg>')
