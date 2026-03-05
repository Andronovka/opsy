import ctypes
import sys

if len(sys.argv) != 5:
    print("Usage: test.py <lib> <key> <input> <output>")
    sys.exit(1)

lib = ctypes.CDLL(sys.argv[1])
key = int(sys.argv[2])

lib.cezare_key.argtypes = [ctypes.c_char]
lib.cezare_key(chr(key).encode('latin1')[0])

lib.cezare_enc.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]

with open(sys.argv[3], 'rb') as f:
    data = f.read()

in_buf = ctypes.create_string_buffer(data, len(data))
out_buf = ctypes.create_string_buffer(len(data))

lib.cezare_enc(in_buf, out_buf, len(data))

with open(sys.argv[4], 'wb') as f:
    f.write(out_buf.raw[:len(data)])

print(f"Done: {sys.argv[3]} -> {sys.argv[4]}")