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

#define BUFFER_SIZE 4096
#define MAX_FILES 100
#define NUM_THREADS 3
#define DEADLOCK_TIMEOUT_SEC 5
#define BUFFER_SIZE 4096
#define WORKERS_COUNT 4

// Структура состояния — создаётся в main и передается по указателю
typedef struct {
    char **files;
    int total_files;
    int next_file_idx;
    int processed_count;
    FILE *log_file;
    const char *output_dir;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} AppState;

// Единственная разрешённая глобальная переменная для сигналов
volatile sig_atomic_t stop = 0;

void handle_sigint(int sig) {
    (void)sig;
    stop = 1;
}

void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Передаем state как аргумент, а не берём из глобальной области
void log_operation(AppState *state, const char *filename, const char *result, long exec_time_ms) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    pthread_mutex_lock(&state->mutex);
    if (state->log_file) {
        fprintf(state->log_file, "%s | Thread: %lu | File: %s | Result: %s | Time: %ld ms\n",
                timestamp, (unsigned long)pthread_self(), filename, result, exec_time_ms);
        fflush(state->log_file);
    }
    pthread_mutex_unlock(&state->mutex);
}

// Логика обработки файла (передаём output_dir параметром)
long process_file_logic(const char *filename, const char *out_dir) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    FILE *in = fopen(filename, "rb");
    if (!in) return -1;

    unsigned char *buffer = malloc(BUFFER_SIZE);
    const char *base_name = strrchr(filename, '/');
    base_name = base_name ? base_name + 1 : filename;

    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s", out_dir, base_name);

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        free(buffer);
        fclose(in);
        return -1;
    }

    int bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, in)) > 0) {
        cezare_enc(buffer, buffer, bytes);
        fwrite(buffer, 1, bytes, out);
    }

    free(buffer);
    fclose(in);
    fclose(out);

    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
}

// Создание выходной директории с проверкой существования
int create_output_dir(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        return mkdir(dir, 0700);
    }
    return 0;  // директория уже существует
}

// Безопасный захват мьютекса с таймаутом (защита от взаимоблокировок)
int safe_mutex_lock(pthread_mutex_t *mutex) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += DEADLOCK_TIMEOUT_SEC;
    
    int ret = pthread_mutex_timedlock(mutex, &ts);
    if (ret == ETIMEDOUT) {
        fprintf(stderr, "Возможная взаимоблокировка: поток %lu ожидает мьютекс более %d секунд\n",
                (unsigned long)pthread_self(), DEADLOCK_TIMEOUT_SEC);
        pthread_mutex_lock(mutex);  // принудительный захват
    }
    return ret;
}

// Рабочий поток
void *worker_thread(void *arg) {
    AppState *state = (AppState *)arg;
    while (!stop) {
        char *filename = NULL;

        pthread_mutex_lock(&state->mutex);
        if (state->next_file_idx >= state->total_files) {
            pthread_mutex_unlock(&state->mutex);
            break;
        }
        filename = state->files[state->next_file_idx++];
        pthread_mutex_unlock(&state->mutex);

        long ms = process_file_logic(filename, state->output_dir);
        
        if (ms >= 0) {
            log_operation(state, filename, "SUCCESS", ms);
            pthread_mutex_lock(&state->mutex);
            state->processed_count++;
            printf("\rОбработано файлов: %d", state->processed_count);
            fflush(stdout);
            pthread_mutex_unlock(&state->mutex);
        }
    }
    return NULL;
}

void run_processing(AppState *state, int is_parallel, long *total_time) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    state->next_file_idx = 0;
    state->processed_count = 0;

    if (!is_parallel) {
        for (int i = 0; i < state->total_files && !stop; i++) {
            long ms = process_file_logic(state->files[i], state->output_dir);
            state->processed_count++;
            log_operation(state, state->files[i], "SUCCESS", ms);
            printf("\rОбработано файлов: %d", state->processed_count);
            fflush(stdout);
        }
    } else {
        pthread_t workers[WORKERS_COUNT];
        for (int i = 0; i < WORKERS_COUNT; i++) 
            pthread_create(&workers[i], NULL, worker_thread, state);
        for (int i = 0; i < WORKERS_COUNT; i++) 
            pthread_join(workers[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    *total_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Использование: %s [--mode=sequential|parallel] file1 file2 ... output_dir/ key\n", argv[0]);
        return 1;
    }

    int mode_flag = 0; // 0 - auto
    int file_start_idx = 1;

    if (strncmp(argv[1], "--mode=", 7) == 0) {
        if (strcmp(argv[1], "--mode=sequential") == 0) mode_flag = 1;
        else if (strcmp(argv[1], "--mode=parallel") == 0) mode_flag = 2;
        else {
            fprintf(stderr, "Ошибка: Неверный режим работы!\n");
            return 1;
        }
        file_start_idx = 2;
    }

    // Инициализация локального состояния внутри main
    AppState state = {0};
    state.total_files = argc - file_start_idx - 2;
    state.files = &argv[file_start_idx];
    state.output_dir = argv[argc - 2];
    int key = atoi(argv[argc - 1]);

    mkdir(state.output_dir, 0700);
    cezare_key((char)key);
    signal(SIGINT, handle_sigint);
    
    pthread_mutex_init(&state.mutex, NULL);
    pthread_cond_init(&state.cond, NULL);
    state.log_file = fopen("log.txt", "a");

    printf("==================================================\n");
    if (mode_flag == 0) {
        int use_parallel = (state.total_files < 5) ? 0 : 1; 
        long t_main, t_alt;

        printf("Авто-режим: %s\n", use_parallel ? "parallel" : "sequential");
        run_processing(&state, use_parallel, &t_main);
        
        printf("\nЗапуск альтернативного прогона\n");
        run_processing(&state, !use_parallel, &t_alt);

        printf("\nТаблица сравнения:\n");
        printf("Режим\t\t| Общее время\t| Среднее на файл\n");
        printf("%s (*)\t| %ld ms\t| %.2f ms\n", use_parallel ? "parallel" : "sequential", t_main, (double)t_main/state.total_files);
        printf("%s\t| %ld ms\t| %.2f ms\n", use_parallel ? "sequential" : "parallel", t_alt, (double)t_alt/state.total_files);
    } else {
        long t;
        run_processing(&state, mode_flag == 2, &t);
        printf("\nЗавершено за %ld ms (среднее: %.2f ms)\n", t, (double)t/state.total_files);
    }

    if (state.log_file) fclose(state.log_file);
    pthread_mutex_destroy(&state.mutex);
    pthread_cond_destroy(&state.cond);

    return 0;
}