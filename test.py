import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect(('127.0.0.1', 8000))
    print("Подключился! Жду данных...")
    while True:
        data = s.recv(1024)
        if not data: break
        print(f"Пришло от сервера: {data.decode().strip()}")