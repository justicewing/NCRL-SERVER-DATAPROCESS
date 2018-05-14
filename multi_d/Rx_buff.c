#include "config.h"
#include <mkl.h>
#include <pthread.h>

extern unsigned char *mbuf;
int index_write_rx;
extern int readyNum_rx;
extern int CacheNum_rx;
extern struct package_t *package_rx;
extern pthread_mutex_t mutex_readyNum_rx;

extern int buffisEmpty;
extern pthread_mutex_t mutex_buffisEmpty;

int buff_to_package(struct package_t *package, unsigned char *buff_p);

void Rx_buff(void *arg)
{
    for (int p = 0; p < CacheNum_rx; p++)
    {
        package_rx[p].tbs = (int *)malloc(sizeof(int) * MaxBeam);
        package_rx[p].CQI_index = (int *)malloc(sizeof(int) * MaxBeam);
        package_rx[p].y = (lapack_complex_float *)malloc(sizeof(SIZE_Y));
        for (int j = 0; j < MaxBeam; j++)
            package_rx[p].data[j] =
                (uint8_t *)malloc(sizeof(uint8_t) * MAX_DATA_SIZE_TX +
                                  crc_length);
    }
    while (1)
    {
        if (readyNum_rx <= CacheNum_rx && !buffisEmpty)
        {
            buff_to_package(&package_rx[index_write_rx], mbuf);
            index_write_rx++;
            if (index_write_rx >= CacheNum_rx)
                index_write_rx = 0;
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
        ((unsigned char *)(package->tbs))[i] = *buff_p;
        buff_p++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(package->CQI_index); i++)
    {
        ((unsigned char *)(package->tbs))[i] = *buff_p;
        buff_p++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(&(package->SNR)); i++)
    {
        *buff_p = ((unsigned char *)(&(package->SNR)))[i];
        buff_p++;
        buff_length++;
    }

    for (int i = 0; i < sizeof(package->y); i++)
    {
        ((unsigned char *)(package->y))[i] = *buff_p;
        buff_p++;
        buff_length++;
    }

    for (int j = 0; j < MaxBeam; j++)
        for (int i = 0; i < sizeof(package->data[j]); i++)
        {
            ((unsigned char *)(package->data[j]))[i] = *buff_p;
            buff_p++;
            buff_length++;
        }

    return buff_length;
}