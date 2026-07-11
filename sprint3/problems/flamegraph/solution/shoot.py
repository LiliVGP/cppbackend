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

SHOOT_COUNT = 500  # Ещё больше выстрелов
COOLDOWN = 0.02    # Минимальная задержка


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
print(f"Server command: {server_cmd}")

# Запускаем сервер с возможностью читать stderr для отладки
server = subprocess.Popen(
    shlex.split(server_cmd),
    stdout=subprocess.DEVNULL,
    stderr=subprocess.PIPE,
    text=True
)
time.sleep(1)  # Даём серверу время на запуск

# Проверяем, что сервер запущен и жив
if server.poll() is not None:
    print(f"ERROR: Server died immediately! Return code: {server.returncode}")
    # Читаем stderr для отладки
    stderr_output = server.stderr.read()
    if stderr_output:
        print(f"Server stderr: {stderr_output}")
    sys.exit(1)

server_pid = server.pid
print(f"Server PID: {server_pid}")

# Даём серверу больше времени на инициализацию
time.sleep(2)

# Проверяем, что сервер всё ещё жив
if server.poll() is not None:
    print(f"ERROR: Server died after initialization! Return code: {server.returncode}")
    stderr_output = server.stderr.read()
    if stderr_output:
        print(f"Server stderr: {stderr_output}")
    sys.exit(1)

# Пытаемся сделать тестовый запрос, чтобы убедиться, что сервер отвечает
try:
    test_result = subprocess.run(
        ['curl', '-s', '-o', '/dev/null', '-w', '%{http_code}', 'localhost:8080/api/v1/maps'],
        capture_output=True,
        text=True,
        timeout=5
    )
    print(f"Test request status: {test_result.stdout}")
    if test_result.stdout != '200':
        print(f"WARNING: Server returned {test_result.stdout}, expected 200")
except subprocess.TimeoutExpired:
    print("ERROR: Server not responding to test request")
    sys.exit(1)
except Exception as e:
    print(f"ERROR: Test request failed: {e}")
    sys.exit(1)

print("Server is running and responding")

# Запускаем perf record с привязкой к процессу сервера
perf = subprocess.Popen(
    ['perf', 'record', '-o', 'perf.data', '-p', str(server_pid), '-F', '999', '-g', '--', 'sleep', '15'],
    stderr=subprocess.DEVNULL
)

print("Perf recording started")
time.sleep(1)

# Обстреливаем
make_shots()

time.sleep(2)  # Даём perf время на запись

# Останавливаем perf
perf.send_signal(signal.SIGINT)
try:
    perf.wait(timeout=5)
except subprocess.TimeoutExpired:
    print("Perf didn't stop, killing...")
    perf.kill()
    perf.wait()

print("Perf recording stopped")

# Останавливаем сервер
stop(server)
print("Server stopped")

# Проверяем perf.data
if not os.path.exists('perf.data') or os.path.getsize('perf.data') == 0:
    print("ERROR: perf.data is empty or does not exist!")
    if os.path.exists('perf.data'):
        print(f"File size: {os.path.getsize('perf.data')} bytes")
    else:
        print("File does not exist")
    sys.exit(1)

print(f"perf.data size: {os.path.getsize('perf.data')} bytes")

# Строим флеймграф
flamegraph_dir = './FlameGraph'

# Проверяем наличие FlameGraph
if not os.path.exists(flamegraph_dir):
    print(f"ERROR: FlameGraph directory '{flamegraph_dir}' not found!")
    sys.exit(1)

print("Generating flamegraph...")
perf_script = subprocess.Popen(
    ['perf', 'script', '-i', 'perf.data'],
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

# Демангилируем имена функций
cxxfilt = subprocess.Popen(
    ['c++filt'],
    stdin=perf_script.stdout,
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

# Сворачиваем стеки
stackcollapse = subprocess.Popen(
    [f'{flamegraph_dir}/stackcollapse-perf.pl'],
    stdin=cxxfilt.stdout,
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL
)

# Генерируем флеймграф
flamegraph = subprocess.Popen(
    [f'{flamegraph_dir}/flamegraph.pl'],
    stdin=stackcollapse.stdout,
    stdout=open('graph.svg', 'w'),
    stderr=subprocess.DEVNULL
)

# Ждём завершения всех процессов
perf_script.wait()
cxxfilt.wait()
stackcollapse.wait()
flamegraph.wait()

print("Flamegraph generated")

# Проверяем graph.svg
if os.path.exists('graph.svg'):
    with open('graph.svg', 'r') as f:
        content = f.read()
        # Проверяем наличие RequestHandler в разных форматах
        if ('http_handler::RequestHandler' in content or
            'RequestHandler' in content or
            '_ZN12http_handler14RequestHandler' in content):
            print("SUCCESS: graph.svg contains RequestHandler functions")
        else:
            # Выводим для отладки
            print("WARNING: RequestHandler not found. First 30 lines:")
            lines = content.split('\n')
            for i, line in enumerate(lines[:30]):
                print(f"{i}: {line}")
            
            # Проверим, есть ли вообще какие-то функции
            if 'function' in content.lower() or 'frame' in content.lower():
                print("Graph contains some function names")
            else:
                print("Graph may not contain function names")
else:
    print("ERROR: graph.svg was not created!")
    sys.exit(1)

print('Job done')
