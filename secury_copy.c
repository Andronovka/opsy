#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "caesar.h"

#define BUFFER_SIZE 4096

volatile int keep_running = 1;

typedef struct {
    unsigned char buffer[BUFFER_SIZE];
    int data_size;
    int full;
    pthread_mutex_t mutex;
    pthread_cond_t can_produce;
    pthread_cond_t can_consume;
} shared_buffer;

typedef struct {
    FILE *in;
    FILE *out;
    shared_buffer *buf;
    long total_size;
    long processed;
} context;

void sigint_handler(int sig)
{
    keep_running = 0;
}

void print_progress(long processed, long total)
{
    static int last = -1;

    int percent = (processed * 100) / total;

    if (percent / 10 != last) {
        last = percent / 10;

        printf("\r[");

        for(int i=0;i<10;i++)
        {
            if(i < percent/10) printf("=");
            else printf(" ");
        }

        printf("] %d%%", percent);
        fflush(stdout);
    }
}

void* producer(void* arg)
{
    context *ctx = arg;

    while(keep_running)
    {
        pthread_mutex_lock(&ctx->buf->mutex);

        while(ctx->buf->full && keep_running)
            pthread_cond_wait(&ctx->buf->can_produce,&ctx->buf->mutex);

        if(!keep_running){
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        int read_bytes = fread(ctx->buf->buffer,1,BUFFER_SIZE,ctx->in);

        if(read_bytes <= 0){
            ctx->buf->data_size = 0;
            ctx->buf->full = 1;
            pthread_cond_signal(&ctx->buf->can_consume);
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        cezare_enc(ctx->buf->buffer, ctx->buf->buffer, read_bytes);

        ctx->buf->data_size = read_bytes;
        ctx->buf->full = 1;

        pthread_cond_signal(&ctx->buf->can_consume);

        pthread_mutex_unlock(&ctx->buf->mutex);
    }

    return NULL;
}

void* consumer(void* arg)
{
    context *ctx = arg;

    while(keep_running)
    {
        pthread_mutex_lock(&ctx->buf->mutex);

        while(!ctx->buf->full && keep_running)
            pthread_cond_wait(&ctx->buf->can_consume,&ctx->buf->mutex);

        if(!keep_running){
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        if(ctx->buf->data_size == 0){
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        fwrite(ctx->buf->buffer,1,ctx->buf->data_size,ctx->out);

        ctx->processed += ctx->buf->data_size;

        print_progress(ctx->processed, ctx->total_size);

        ctx->buf->full = 0;

        pthread_cond_signal(&ctx->buf->can_produce);

        pthread_mutex_unlock(&ctx->buf->mutex);
    }

    return NULL;
}

int main(int argc,char* argv[])
{
    if(argc != 4)
    {
        printf("Usage: %s input output key\n",argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1],"rb");

    if(!in){
        printf("Input file error\n");
        return 1;
    }

    FILE *out = fopen(argv[2],"wb");

    if(!out){
        printf("Output file error\n");
        fclose(in);
        return 1;
    }

    int key = atoi(argv[3]);
    cezare_key(key);

    signal(SIGINT, sigint_handler);

    fseek(in,0,SEEK_END);
    long size = ftell(in);
    rewind(in);

    shared_buffer buf;

    buf.data_size = 0;
    buf.full = 0;

    pthread_mutex_init(&buf.mutex,NULL);
    pthread_cond_init(&buf.can_produce,NULL);
    pthread_cond_init(&buf.can_consume,NULL);

    context ctx;

    ctx.in = in;
    ctx.out = out;
    ctx.buf = &buf;
    ctx.total_size = size;
    ctx.processed = 0;

    pthread_t prod, cons;

    pthread_create(&prod,NULL,producer,&ctx);
    pthread_create(&cons,NULL,consumer,&ctx);

    pthread_join(prod,NULL);
    pthread_join(cons,NULL);

    fclose(in);
    fclose(out);

    if(!keep_running)
    {
        printf("\nОперация прервана пользователем\n");
        remove(argv[2]);
        return 1;
    }

    printf("\nDone\n");

    return 0;
}