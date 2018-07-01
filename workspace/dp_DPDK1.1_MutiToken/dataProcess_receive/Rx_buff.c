#include "config.h"
#include <mkl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

extern unsigned char *mbuf;
extern int readyNum_rx;
struct package_t package_rx[PACK_CACHE];
extern pthread_mutex_t mutex_readyNum_rx;
extern sem_t rx_can_be_destroyed;
extern sem_t rx_prepared;
extern sem_t rx_buff_prepared;
extern int buffisEmpty;
extern pthread_mutex_t mutex_buffisEmpty;

int index_write_rx;
int buff_to_package(struct package_t *package, unsigned char *buff_p);

void Rx_buff(void *arg)
{
    // printf("rx buff start\n");
    for (int p = 0; p < PACK_CACHE; p++)
    {
        package_rx[p].tbs = (int *)malloc(sizeof(int) * MAX_BEAM);
        package_rx[p].CQI_index = (int *)malloc(sizeof(int) * MAX_BEAM);
        package_rx[p].y = (lapack_complex_float *)malloc(SIZE_Y);
        for (int j = 0; j < MAX_BEAM; j++)
            package_rx[p].data[j] =
                (uint8_t *)malloc(sizeof(uint8_t) * MAX_DATA_LEN_TX +
                                  CRC_LENGTH);
    }
    printf("rx buff is ready...\n");
    sem_post(&rx_buff_prepared);
    sem_wait(&rx_prepared);
    printf("rx buff start\n");
    while (1)
    {
        if (readyNum_rx <= PACK_CACHE && (!buffisEmpty))
        {
            printf("rx buff circle start\n");
            buff_to_package(&package_rx[index_write_rx], mbuf);
            index_write_rx++;
            if (index_write_rx >= PACK_CACHE)
                index_write_rx = 0;
            pthread_mutex_lock(&mutex_readyNum_rx);
            readyNum_rx++;
            pthread_mutex_unlock(&mutex_readyNum_rx);
            pthread_mutex_lock(&mutex_buffisEmpty);
            buffisEmpty = 1;
            pthread_mutex_lock(&mutex_buffisEmpty);
        }
    }
    sem_post(&rx_can_be_destroyed);
}

int buff_to_package(struct package_t *package, unsigned char *buff)
{
    unsigned char *buff_p = buff;
    int buff_length = 0;

    uint8_t *tbs_p = (uint8_t *)(package->tbs);
    for (int i = 0; i < MAX_BEAM; i++)
    {
        *tbs_p = *buff_p;
        buff_p++;
        tbs_p++;
        buff_length++;
    }
    // printf("tbs done\n");

    uint8_t *cqi_p = (uint8_t *)(package->CQI_index);
    for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
    {
        *cqi_p = *buff_p;
        buff_p++;
        cqi_p++;
        buff_length++;
    }
    // printf("cqi done\n");

    uint8_t *snr_p = (uint8_t *)(&(package->SNR));
    for (int i = 0; i < sizeof(float); i++)
    {
        *snr_p = *buff_p;
        buff_p++;
        snr_p++;
        buff_length++;
    }
    printf("snr done\n");

    uint8_t *y_p = (uint8_t *)(package->y);
    for (int i = 0; i < SIZE_Y; i++)
    {
        *y_p = *buff_p;
        buff_p++;
        y_p++;
        buff_length++;
    }
    // printf("y done\n");
    uint8_t *data_p;
    // printf("buff_length:%d\n",buff_length);
    unsigned char tem;
    for (int j = 0; j < MAX_BEAM; j++)
    {
        // data_p = (uint8_t *)(package->data[j]);
        for (int i = 0; i < MAX_DATA_LEN_TX + CRC_LENGTH; i++)
        {
            // tem = 0xFF;
            // *buff_p = tem;
            package->data[j][i] = *buff_p;
            buff_p++;
            data_p++;
            // printf("buff_length:%d\n",buff_length);
            buff_length++;
        }
        // printf("data%d done\n", j);

    } // printf("data done\n");

    return buff_length;
}