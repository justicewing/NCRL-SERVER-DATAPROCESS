#include "config.h"
#include <mkl.h>
#include <pthread.h>

#define MAX_MBUFF 2 * MaxBeam * sizeof(int) + sizeof(float) + \
                      MaxBeam *CarrierNum *SymbolNum * sizeof(lapack_complex_float)

extern unsigned char *mbuf;
extern int index_write_rx;
extern int readyNum_rx;
extern int CacheNum_rx;
extern struct package_t *package_rx;
extern pthread_mutex_t mutex_readyNum_rx;

extern int buffisEmpty;
extern pthread_mutex_t mutex_buffisEmpty;

int buff_to_package(struct package_t *package, unsigned char *buff);

void Rx_buff(void *arg)
{
    while (1)
    {
        if (readyNum_rx <= CacheNum_rx && !buffisEmpty)
        {
            buff_to_package(&package_rx[index_write_rx], mbuf);
            pthread_mutex_lock(&mutex_readyNum_rx);
            readyNum_rx++;
            pthread_mutex_unlock(&mutex_readyNum_rx);
            pthread_mutex_lock(&mutex_buffisEmpty);
            buffisEmpty = 1;
            pthread_mutex_lock(&mutex_buffisEmpty);
        }
    }
}

int buff_to_package(struct package_t *package, unsigned char *buff)
{
    unsigned char *buff_p = buff;
    int buff_length = 0;

    for (int i = 0; i < sizeof(package->tbs); i++)
    {
        ((unsigned char *)(package->tbs))[i] = *buff;
        buff++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(package->CQI_index); i++)
    {
        ((unsigned char *)(package->tbs))[i] = *buff;
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
        ((unsigned char *)(package->y))[i] = *buff;
        buff++;
        buff_length++;
    }

    return buff_length;
}