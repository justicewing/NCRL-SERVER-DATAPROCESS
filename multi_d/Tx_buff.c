#include "config.h"
#include <mkl.h>
#include <pthread.h>

#define MAX_MBUFF 2 * MaxBeam * sizeof(int) + sizeof(float) + \
                      MaxBeam *CarrierNum *SymbolNum * sizeof(lapack_complex_float)
unsigned char mbuf[MAX_MBUFF];

int index_read_tx;
extern int readyNum_tx;
extern const int packCache_tx;
extern struct package_t *package_tx;
extern pthread_mutex_t mutex_readyNum_tx;

int buffisEmpty = 1;
pthread_mutex_t mutex_buffisEmpty;

int package_to_buff(struct package_t *package, unsigned char *buff);

void Tx_buff(void *arg)
{
    index_read_tx = 0;
    pthread_mutex_init(&mutex_buffisEmpty, NULL);
    while (1)
    {
        if (readyNum_tx > 0 && buffisEmpty)
        {
            package_to_buff(&package_tx[index_read_tx], mbuf);
            index_read_tx++;
            if (index_read_tx == packCache_tx)
                index_read_tx = 0;
            pthread_mutex_lock(&mutex_readyNum_tx);
            readyNum_tx--;
            pthread_mutex_unlock(&mutex_readyNum_tx);
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