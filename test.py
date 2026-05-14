import ctypes
import os
import random
import tempfile

LIB_PATH = "./libcaesar.so"

def load_lib():
    lib = ctypes.CDLL(LIB_PATH)

    lib.cezare_key.argtypes = [ctypes.c_char]
    lib.cezare_key.restype = None

    lib.cezare_enc.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int]
    lib.cezare_enc.restype = None

    return lib


def encrypt(lib, data: bytes, key: int) -> bytes:
    lib.cezare_key(key)

    in_buf = ctypes.create_string_buffer(data, len(data))
    out_buf = ctypes.create_string_buffer(len(data))

    lib.cezare_enc(in_buf, out_buf, len(data))

    return out_buf.raw[:len(data)]


def test_basic():
    print("Test: basic")

    lib = load_lib()
    data = b"hello world"
    key = 42

    enc = encrypt(lib, data, key)
    dec = encrypt(lib, enc, key)

    assert dec == data, "Basic test failed"
    print("OK")


def test_empty():
    print("Test: empty")

    lib = load_lib()
    data = b""
    key = 10

    enc = encrypt(lib, data, key)
    assert enc == b""

    print("OK")


def test_random():
    print("Test: random data")

    lib = load_lib()

    for _ in range(10):
        size = random.randint(1, 10000)
        data = os.urandom(size)
        key = random.randint(0, 255)

        enc = encrypt(lib, data, key)
        dec = encrypt(lib, enc, key)

        assert dec == data, f"Random test failed (size={size}, key={key})"

    print("OK")


def test_all_keys():
    print("Test: all keys")

    lib = load_lib()
    data = b"test_data_123"

    for key in range(256):
        enc = encrypt(lib, data, key)
        dec = encrypt(lib, enc, key)

        assert dec == data, f"Key {key} failed"

    print("OK")


def test_files():
    print("Test: file processing")

    lib = load_lib()

    with tempfile.TemporaryDirectory() as tmp:
        input_path = os.path.join(tmp, "input.bin")
        output_path = os.path.join(tmp, "output.bin")
        restored_path = os.path.join(tmp, "restored.bin")

        data = os.urandom(5000)

        with open(input_path, "wb") as f:
            f.write(data)

        key = 77

        # encrypt
        enc = encrypt(lib, data, key)
        with open(output_path, "wb") as f:
            f.write(enc)

        # decrypt
        dec = encrypt(lib, enc, key)
        with open(restored_path, "wb") as f:
            f.write(dec)

        assert dec == data, "File test failed"

    print("OK")


if __name__ == "__main__":
    test_basic()
    test_empty()
    test_random()
    test_all_keys()
    test_files()

    print("\nALL TESTS PASSED")