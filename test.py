import ctypes
import sys
import os
from pathlib import Path

# Проверка аргументов
if len(sys.argv) != 5:
    print("Использование: test.py <lib> <key> <input> <output>")
    print("Пример: test.py ./libcaesar.so 42 input.txt output.txt")
    sys.exit(1)

lib_path, key_str, input_file, output_file = sys.argv[1:5]

# Проверка ключа
try:
    key = int(key_str)
    if key < 0 or key > 255:
        print(f"Ошибка: ключ должен быть от 0 до 255, получено {key}")
        sys.exit(1)
except ValueError:
    print(f"Ошибка: ключ должен быть числом, получено '{key_str}'")
    sys.exit(1)

# Проверка входного файла
if not os.path.exists(input_file):
    print(f"Ошибка: входной файл '{input_file}' не найден")
    sys.exit(1)

try:
    # Загрузка библиотеки
    lib = ctypes.CDLL(lib_path)
    
    # Настройка типов аргументов
    lib.cezare_key.argtypes = [ctypes.c_char]
    lib.cezare_key.restype = None
    
    lib.cezare_enc.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    lib.cezare_enc.restype = None
    
    # Установка ключа
    lib.cezare_key(chr(key).encode('latin1')[0])
    
    # Чтение входного файла
    with open(input_file, 'rb') as f:
        data = f.read()
    
    # Шифрование
    in_buf = ctypes.create_string_buffer(data, len(data))
    out_buf = ctypes.create_string_buffer(len(data))
    lib.cezare_enc(in_buf, out_buf, len(data))
    
    # Создание выходной директории
    Path(output_file).parent.mkdir(parents=True, exist_ok=True)
    
    # Запись результата
    with open(output_file, 'wb') as f:
        f.write(out_buf.raw[:len(data)])
    
    print(f"Готово: {input_file} -> {output_file}")
    
except ctypes.CDLLError as e:
    print(f"Ошибка загрузки библиотеки '{lib_path}': {e}")
    sys.exit(1)
except Exception as e:
    print(f"Ошибка: {e}")
    sys.exit(1)