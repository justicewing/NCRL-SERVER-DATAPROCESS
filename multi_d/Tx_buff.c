#include "config.h"
#include <mkl.h>
#include <pthread.h>

#define MAX_MBUFF 2 * MaxBeam * sizeof(int) + sizeof(float) + \
                      MaxBeam *CarrierNum *SymbolNum * sizeof(lapack_complex_float)
unsigned char mbuf[MAX_MBUFF];

extern int index_cache;
extern int readyNum;
extern const int packCache;
extern struct package_t *package_test;
extern pthread_mutex_t mutex_readyNum;

int buffisEmpty = 1;
pthread_mutex_t mutex_buffisEmpty;

int package_to_buff(struct package_t *package, unsigned char *buff);

void Tx_buff(void *arg)
{
    pthread_mutex_init(&mutex_buffisEmpty, NULL);
    while (1)
    {
        if (readyNum > 0 && buffisEmpty)
        {
            package_to_buff(&package_test[index_cache], mbuf);
            pthread_mutex_lock(&mutex_readyNum);
            readyNum--;
            pthread_mutex_unlock(&mutex_readyNum);
            pthread_mutex_lock(&mutex_buffisEmpty);
            buffisEmpty = 0;
            pthread_mutex_lock(&mutex_buffisEmpty);
        }
    }
    pthread_mutex_destroy(&mutex_buffisEmpty);
}

int package_to_buff(struct package_t *package, unsigned char *buff)
{
    unsigned char *buff_p = buff;
    int buff_length = 0;

    for (int i = 0; i < sizeof(package->tbs); i++)
    {
        *buff = ((unsigned char *)(package->tbs))[i];
        buff++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(package->CQI_index); i++)
    {
        *buff = ((unsigned char *)(package->tbs))[i];
        buff++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(&(package->SNR)); i++)
    {
        *buff = ((unsigned char *)(&(package->SNR)))[i];
        buff++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(package->y); i++)
    {
        *buff = ((unsigned char *)(package->y))[i];
        buff++;
        buff_length++;
    }

    return buff_length;
}