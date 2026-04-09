<?php
$message = "";

// Проверяем, была ли нажата кнопка отправки
if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $cmd = $_POST['c'] ?? '';
    $val = $_POST['v'] ?? '';

    if (!empty($cmd)) {
        // Формируем URL для нашего Python сервера
        $url = "http://127.0.0.1:5000/cmd?c=" . urlencode($cmd) . "&v=" . urlencode($val);
        
        // Отправляем запрос и получаем ответ
        $response = @file_get_contents($url);
        
        if ($response !== false) {
            $message = "<div class='success'>Команда [$cmd:$val] отправлена!</div>";
        } else {
            $message = "<div class='error'>Ошибка: Python-сервер недоступен.</div>";
        }
    }
}
?>

<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Control Panel</title>
    <style>
        body { background: #0f0f0f; color: #00ff41; font-family: 'Courier New', monospace; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .panel { background: #1a1a1a; padding: 30px; border: 2px solid #00ff41; border-radius: 10px; box-shadow: 0 0 20px rgba(0, 255, 65, 0.2); width: 320px; }
        h2 { text-align: center; text-transform: uppercase; letter-spacing: 2px; }
        label { display: block; margin: 15px 0 5px; font-size: 0.8em; }
        select, input { width: 100%; padding: 10px; background: #222; border: 1px solid #00ff41; color: #00ff41; border-radius: 5px; box-sizing: border-box; }
        button { width: 100%; margin-top: 20px; padding: 12px; background: #00ff41; color: #000; border: none; border-radius: 5px; font-weight: bold; cursor: pointer; transition: 0.3s; }
        button:hover { background: #00cc33; box-shadow: 0 0 15px #00ff41; }
        .success { color: #00ff41; text-align: center; margin-top: 15px; font-size: 0.9em; }
        .error { color: #ff3333; text-align: center; margin-top: 15px; font-size: 0.9em; }
    </style>
</head>
<body>

<div class="panel">
    <h2>ESP32 Link</h2>
    
    <form method="POST">
        <label>Выберите устройство:</label>
        <select name="c">
            <option value="Lampa">Лампа в зале</option>
            <option value="Relay1">Реле 1 (Гараж)</option>
            <option value="Servo">Привод ворот</option>
            <option value="Alarm">Сигнализация</option>
        </select>

        <label>Действие / Параметр:</label>
        <select name="v">
            <option value="ON">Включить (ON)</option>
            <option value="OFF">Выключить (OFF)</option>
            <option value="OPEN">Открыть (OPEN)</option>
            <option value="CLOSE">Закрыть (CLOSE)</option>
        </select>

        <button type="submit">ВЫПОЛНИТЬ КОМАНДУ</button>
    </form>

    <?php echo $message; ?>
</div>

</body>
</html>