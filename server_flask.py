import socket
import threading
from flask import Flask, request

app = Flask(__name__)
# Тут храним последнюю команду, пока ESP её не заберет
cmd_queue = [] 

# --- TCP СЕРВЕР ДЛЯ ESP32 ---
def esp32_tcp_handler():
    global cmd_queue
    # Создаем сокет (IPv4, TCP)
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(('0.0.0.0', 8000)) # Порт для ESP32
        s.listen(1)
        
        while True:
            print("Ожидание подключения ESP32...")
            conn, addr = s.accept()
            print("Ожиданиt:")
            with conn:
                print(f"ESP32 подключена: {addr}")
                conn.setblocking(False) # Не ждем вечно
                
                while True:
                    # Если в очереди есть команды - отправляем их все
                    while cmd_queue:
                        command = cmd_queue.pop(0)
                        try:
                            conn.sendall(command.encode() + b'\n')
                            print(f"Отправлено на ESP32: {command}")
                        except:
                            print("Ошибка отправки, ESP32 отвалилась")
                            break
                    
                    # Проверяем, жива ли еще связь (короткий recv)
                    try:
                        data = conn.recv(1024)
                        if not data: break # Если пришел пустой пакет - дисконнект
                    except BlockingIOError:
                        pass # Данных нет, это нормально
                    except Exception:
                        break
                    
                    import time
                    time.sleep(0.1) # Чтобы не раскалять процессор

# --- WEB ИНТЕРФЕЙС ДЛЯ ANDROID ---
@app.route('/cmd')
def web_trigger():
    c = request.args.get('c', '0') # Номер команды
    v = request.args.get('v', '0') # Значение (параметр)
    
    formatted_cmd = f"{c}:{v}"
    cmd_queue.append(formatted_cmd)
    
    return f"<h3>Команда [{formatted_cmd}] в очереди!</h3><p>ESP32 заберет её через мгновение.</p>"

if __name__ == "__main__":
    # Запускаем TCP сервер в отдельном потоке
    threading.Thread(target=esp32_tcp_handler, daemon=True).start()
    # Запускаем Flask на порту 5000
    app.run(host='0.0.0.0', port=5000)