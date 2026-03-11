#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "caesar.h"

#define BUFFER_SIZE 4096

volatile sig_atomic_t stop = 0;

typedef struct {
    unsigned char buffer[BUFFER_SIZE];
    int size;
    int full;
    pthread_mutex_t mutex;
    pthread_cond_t produce;
    pthread_cond_t consume;
} shared_buffer;

typedef struct {
    FILE *in;
    FILE *out;
    long total;
    long processed;
    shared_buffer *buf;
} context;

void handle_sigint(int sig)
{
    stop = 1;
}

void print_progress(long done,long total)
{
    int percent = (done * 100) / total;

    printf("\rProgress: %d%%",percent);
    fflush(stdout);
}

void* producer(void *arg)
{
    context *ctx = arg;

    while(!stop)
    {
        pthread_mutex_lock(&ctx->buf->mutex);

        while(ctx->buf->full && !stop)
            pthread_cond_wait(&ctx->buf->produce,&ctx->buf->mutex);

        if(stop){
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        int n = fread(ctx->buf->buffer,1,BUFFER_SIZE,ctx->in);

        if(n <= 0){
            ctx->buf->size = 0;
            ctx->buf->full = 1;
            pthread_cond_signal(&ctx->buf->consume);
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        cezare_enc(ctx->buf->buffer,ctx->buf->buffer,n);

        ctx->buf->size = n;
        ctx->buf->full = 1;

        pthread_cond_signal(&ctx->buf->consume);

        pthread_mutex_unlock(&ctx->buf->mutex);
    }

    pthread_cond_signal(&ctx->buf->consume);

    return NULL;
}

void* consumer(void *arg)
{
    context *ctx = arg;

    while(!stop)
    {
        pthread_mutex_lock(&ctx->buf->mutex);

        while(!ctx->buf->full && !stop)
            pthread_cond_wait(&ctx->buf->consume,&ctx->buf->mutex);

        if(stop){
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        if(ctx->buf->size == 0){
            pthread_mutex_unlock(&ctx->buf->mutex);
            break;
        }

        fwrite(ctx->buf->buffer,1,ctx->buf->size,ctx->out);

        ctx->processed += ctx->buf->size;

        print_progress(ctx->processed,ctx->total);

        ctx->buf->full = 0;

        pthread_cond_signal(&ctx->buf->produce);

        pthread_mutex_unlock(&ctx->buf->mutex);
    }

    pthread_cond_signal(&ctx->buf->produce);

    return NULL;
}

int main(int argc,char *argv[])
{
    if(argc != 4)
    {
        printf("Usage: %s input output key\n",argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1],"rb");
    if(!in){
        printf("Cannot open input file\n");
        return 1;
    }

    FILE *out = fopen(argv[2],"wb");
    if(!out){
        printf("Cannot open output file\n");
        fclose(in);
        return 1;
    }

    int key = atoi(argv[3]);
    cezare_key(key);

    signal(SIGINT,handle_sigint);

    fseek(in,0,SEEK_END);
    long size = ftell(in);
    rewind(in);

    shared_buffer buf;

    buf.size = 0;
    buf.full = 0;

    pthread_mutex_init(&buf.mutex,NULL);
    pthread_cond_init(&buf.produce,NULL);
    pthread_cond_init(&buf.consume,NULL);

    context ctx;

    ctx.in = in;
    ctx.out = out;
    ctx.total = size;
    ctx.processed = 0;
    ctx.buf = &buf;

    pthread_t t1,t2;

    pthread_create(&t1,NULL,producer,&ctx);
    pthread_create(&t2,NULL,consumer,&ctx);

    pthread_join(t1,NULL);
    pthread_join(t2,NULL);

    fclose(in);
    fclose(out);

    if(stop)
    {
        printf("\nOperation cancelled\n");
        remove(argv[2]);
        return 1;
    }

    printf("\nDone\n");

    return 0;
}