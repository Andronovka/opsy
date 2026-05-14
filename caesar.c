#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include "caesar.h"

// Указатель на защищенную область памяти
static char* protected_key = NULL;

// Мьютекс для защиты mprotect в многопоточной среде
static pthread_mutex_t key_mutex = PTHREAD_MUTEX_INITIALIZER;

void cezare_key(char new_key) {
    pthread_mutex_lock(&key_mutex);
    
    if (protected_key == NULL) {
        // Выделяем 16 байт приватной анонимной памяти (чтение и запись)
        protected_key = mmap(NULL, 16, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (protected_key == MAP_FAILED) {
            protected_key = NULL;
            pthread_mutex_unlock(&key_mutex);
            return;
        }
    } else {
        // Если ключ переназначается, расширяем права
        mprotect(protected_key, 16, PROT_READ | PROT_WRITE);
    }
    
    // Записываем ключ и устанавливаем права "только чтение"
    memcpy(protected_key, &new_key, 1);
    mprotect(protected_key, 16, PROT_READ);
    
    pthread_mutex_unlock(&key_mutex);
}

void cezare_enc(void* in_data, void* out_data, int data_len) {
    if (in_data == 0 || out_data == 0 || data_len == 0 || protected_key == NULL) {
        return;
    }
    
    pthread_mutex_lock(&key_mutex);
    // Временно разрешаем запись
    mprotect(protected_key, 16, PROT_READ | PROT_WRITE);
    // Копируем ключ в локальную переменную (на стек)
    char local_key = protected_key[0];
    // Снова закрываем память
    mprotect(protected_key, 16, PROT_READ);
    pthread_mutex_unlock(&key_mutex);
    
    unsigned char* reader = (unsigned char*)in_data;
    unsigned char* writer = (unsigned char*)out_data;
    unsigned char* end = reader + data_len;
    
    while (reader < end) {
        *writer = *reader ^ local_key;
        reader++;
        writer++;
    }
}

void cezare_cleanup(void) {
    pthread_mutex_lock(&key_mutex);
    if (protected_key) {
        // Расширяем права для очистки
        mprotect(protected_key, 16, PROT_READ | PROT_WRITE);
        // Затираем память нулями
        memset(protected_key, 0, 16);
        // Возвращаем исходные права (по ТЗ)
        mprotect(protected_key, 16, PROT_READ);
        // Освобождаем память
        munmap(protected_key, 16);
        protected_key = NULL;
    }
    pthread_mutex_unlock(&key_mutex);
}

void* get_protected_key_ptr(void) {
    return protected_key;
}

void test_memory_violation(void) {
    if (protected_key) {
        // Прямая попытка модификации памяти в обход mprotect.
        // Это приведет к аппаратной ошибке и вызову SIGSEGV.
        protected_key[0] = 'X';
    }
}