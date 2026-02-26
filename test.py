import ctypes
import sys

if len(sys.argv) != 5:
    print("Usage: test.py <lib> <key> <input> <output>")
    sys.exit(1)

lib = ctypes.CDLL(sys.argv[1])
key = int(sys.argv[2])

lib.set_key.argtypes = [ctypes.c_int]
lib.set_key(key)

lib.transform.argtypes = [ctypes.c_void_p, ctypes.c_int]

with open(sys.argv[3], 'rb') as f:
    data = f.read()

buf = ctypes.create_string_buffer(data, len(data))
lib.transform(buf, len(data))

with open(sys.argv[4], 'wb') as f:
    f.write(buf.raw[:len(data)])

print(f"Done: {sys.argv[3]} -> {sys.argv[4]}")