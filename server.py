import socket
import time

HOST = '0.0.0.0'  # Слушать все интерфейсы
PORT = 8000       # Порт для ESP32

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen()
        print(f"Сервер запущен на порту {PORT}. Ожидание ESP32...")
        
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"ESP32 подключилась: {addr}")
                conn.settimeout(10.0) # Тайм-аут на чтение (проверка связи)
                try:
                    while True:
                        # Здесь ты можешь вводить команду вручную для теста
                        # В реальности тут будет логика приема из веб-интерфейса
                        cmd = input("Введите команду (или 'wait'): ")
                        if cmd != 'wait':
                            # Отправляем как сырые байты. Напр: "1:255\n"
                            conn.sendall(cmd.encode() + b'\n')
                        
                        # Небольшая пауза, чтобы не забивать цикл
                        time.sleep(0.1)
                except (ConnectionResetError, BrokenPipeError, socket.timeout):
                    print("Связь с ESP32 потеряна.")
                    continue

if __name__ == "__main__":
    start_server()