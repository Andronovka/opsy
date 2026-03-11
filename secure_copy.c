#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "caesar.h"

#define BUFFER_SIZE 4096ы
#define MAX_FILES 100
#define NUM_THREADS 3
#define DEADLOCK_TIMEOUT_SEC 5

volatile sig_atomic_t stop = 0;

// Глобальные ресурсы

// Очередь файлов для обработки
typedef struct {
    char *files[MAX_FILES];
    int count;
    int next;
} file_queue;

// Единое состояние: счетчик + лог + единственный мьютекс
typedef struct {
    int count;              // счетчик обработанных файлов
    FILE *log_file;         // файл лога
    pthread_mutex_t mutex;  // единственный мьютекс в программе
} shared_state;

shared_state state = {0, NULL, PTHREAD_MUTEX_INITIALIZER};
char *output_dir = NULL;
int encryption_key = 0;  // глобальная переменная для ключа

// Вспомогательные функции

void handle_sigint(int sig) {
    (void)sig;
    stop = 1;
    printf("\nОперация отменена пользователем\n");
}

void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Логирование с использованием единого мьютекса и timedlock
void log_operation(const char *filename, const char *result, long exec_time_ms) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += DEADLOCK_TIMEOUT_SEC;
    
    int ret = pthread_mutex_timedlock(&state.mutex, &ts);
    if (ret == ETIMEDOUT) {
        fprintf(stderr, "Возможная взаимоблокировка: поток %lu ожидает мьютекс более %d секунд\n",
                (unsigned long)pthread_self(), DEADLOCK_TIMEOUT_SEC);
        // Повторная попытка без таймаута для гарантированной записи
        pthread_mutex_lock(&state.mutex);
    }
    
    fprintf(state.log_file, "%s | Thread: %lu | File: %s | Result: %s | Time: %ld ms\n",
            timestamp, (unsigned long)pthread_self(), filename, result, exec_time_ms);
    fflush(state.log_file);
    pthread_mutex_unlock(&state.mutex);
}

// Инкремент счетчика с использованием единого мьютекса и timedlock
void increment_counter() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += DEADLOCK_TIMEOUT_SEC;
    
    int ret = pthread_mutex_timedlock(&state.mutex, &ts);
    if (ret == ETIMEDOUT) {
        fprintf(stderr, "Возможная взаимоблокировка: поток %lu ожидает мьютекс более %d секунд\n",
                (unsigned long)pthread_self(), DEADLOCK_TIMEOUT_SEC);
        pthread_mutex_lock(&state.mutex);
    }
    
    state.count++;
    printf("\rОбработано файлов: %d", state.count);
    fflush(stdout);
    
    pthread_mutex_unlock(&state.mutex);
}

// Получение следующего файла из очереди с использованием единого мьютекса
char *get_next_file(file_queue *queue) {
    char *file = NULL;
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += DEADLOCK_TIMEOUT_SEC;
    
    int ret = pthread_mutex_timedlock(&state.mutex, &ts);
    if (ret == ETIMEDOUT) {
        fprintf(stderr, "Возможная взаимоблокировка: поток %lu ожидает мьютекс более %d секунд\n",
                (unsigned long)pthread_self(), DEADLOCK_TIMEOUT_SEC);
        pthread_mutex_lock(&state.mutex);
    }
    
    if (queue->next < queue->count) {
        file = queue->files[queue->next];
        queue->next++;
    }
    
    pthread_mutex_unlock(&state.mutex);
    return file;
}

// Создание выходной директории
int create_output_dir(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        return mkdir(dir, 0700);
    }
    return 0;
}

// Поток обработки файла

void *process_file(void *arg) {
    file_queue *queue = (file_queue *)arg;
    char *filename;
    
    while ((filename = get_next_file(queue)) != NULL && !stop) {
        struct timespec start, end;
        clock_gettime(CLOCK_REALTIME, &start);
        
        // Открываем входной файл
        FILE *in = fopen(filename, "rb");
        if (!in) {
            log_operation(filename, "ERROR: Cannot open input", 0);
            continue;
        }
        
        // Выделяем буфер
        unsigned char *buffer = malloc(BUFFER_SIZE);
        if (!buffer) {
            fclose(in);
            log_operation(filename, "ERROR: Memory allocation failed", 0);
            continue;
        }
        
        // Формируем путь к выходному файлу (сохраняем оригинальное имя)
        char *base_name = strrchr(filename, '/');
        base_name = base_name ? base_name + 1 : filename;
        
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, base_name);
        
        // Открываем выходной файл
        FILE *out = fopen(output_path, "wb");
        if (!out) {
            free(buffer);
            fclose(in);
            log_operation(filename, "ERROR: Cannot create output", 0);
            continue;
        }
        
        // Читаем, шифруем, записываем
        int bytes;
        while ((bytes = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
            cezare_enc(buffer, buffer, bytes);
            fwrite(buffer, 1, bytes, out);
        }
        
        free(buffer);
        fclose(in);
        fclose(out);
        
        // Засекаем время выполнения
        clock_gettime(CLOCK_REALTIME, &end);
        long exec_time_ms = (end.tv_sec - start.tv_sec) * 1000 + 
                           (end.tv_nsec - start.tv_nsec) / 1000000;
        
        // Обновляем счётчик и логируем
        increment_counter();
        log_operation(filename, "SUCCESS", exec_time_ms);
    }
    
    return NULL;
}


int main(int argc, char *argv[]) {
    // Проверка аргументов: минимум 2 файла + выходная директория + ключ
    if (argc < 4) {
        printf("Использование: %s file1.txt file2.txt ... output_dir/ key\n", argv[0]);
        return 1;
    }
    
    // Парсим аргументы: последний — ключ, предпоследний — выходная директория
    encryption_key = atoi(argv[argc - 1]);
    output_dir = argv[argc - 2];
    
    // Инициализируем ключ шифрования
    cezare_key((char)encryption_key);
    
    // Создаём выходную директорию при необходимости
    if (create_output_dir(output_dir) != 0) {
        printf("Ошибка создания директории: %s\n", output_dir);
        return 1;
    }
    
    // Инициализируем очередь файлов
    file_queue queue;
    queue.count = argc - 3;  // все аргументы, кроме output_dir и key
    queue.next = 0;
    for (int i = 0; i < queue.count; i++) {
        queue.files[i] = argv[i + 1];
    }
    
    // Открываем файл лога в режиме append
    state.log_file = fopen("log.txt", "a");
    if (!state.log_file) {
        printf("Ошибка создания log.txt\n");
        return 1;
    }
    
    // Регистрируем обработчик сигнала
    signal(SIGINT, handle_sigint);
    
    printf("Обработка %d файлов (%d потоков)!\n", queue.count, NUM_THREADS);
    printf("Выходная директория: %s, ключ: %d\n", output_dir, encryption_key);
    
    // Создаём 3 потока обработки
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, process_file, &queue);
    }
    
    // Ждём завершения всех потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Закрываем лог
    fclose(state.log_file);
    
    printf("\nГотово! Обработано файлов: %d\n", state.count);
    
    return 0;
}