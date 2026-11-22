
#include <pthread.h>
#include <stdio.h>

enum { THREADS = 2, ITERATIONS = 1000000 };

static long            counter = 0;
static pthread_mutex_t m       = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&m);
        counter++;                    /* критическая секция */
        pthread_mutex_unlock(&m);
    }
    return NULL;
}

int main(void)
{
    pthread_t tid[THREADS];

    for (int i = 0; i < THREADS; i++)
        pthread_create(&tid[i], NULL, worker, NULL);
    for (int i = 0; i < THREADS; i++)
        pthread_join(tid[i], NULL);

    long expected = (long)THREADS * ITERATIONS;
    printf("ожидалось: %ld  получено: %ld  потеряно: %ld\n",
           expected, counter, expected - counter);
    return 0;
}