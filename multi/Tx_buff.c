#include "config.h"
#include <mkl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>

uint8_t *mbuf;
// extern uint8_t *mbuf;

int index_read_tx;
extern int readyNum_tx;
extern int startNum_tx;
extern struct package_t *package_tx;
extern pthread_mutex_t mutex_readyNum_tx;
extern pthread_mutex_t mutex_startNum_tx;
extern sem_t tx_buff_can_be_destroyed;
extern sem_t tx_prepared;
extern sem_t tx_buff_prepared;

int buffisEmpty;
pthread_mutex_t mutex_buffisEmpty;

int package_to_buff(struct package_t *package, uint8_t *buff_p);

void Tx_buff(void *arg)
{
    // printf("tx buff start\n");
    index_read_tx = 0;
    buffisEmpty = 1;
    mbuf = (unsigned char *)malloc(MAX_MBUFF);
    pthread_mutex_init(&mutex_buffisEmpty, NULL);
    printf("Tx buff prepared...\n");
    sem_post(&tx_buff_prepared);
    sem_wait(&tx_prepared);
    printf("tx buff start\n");
    while (1)
    {
        printf("aaaaaa……\n");
        if (readyNum_tx > 0 && buffisEmpty)
        {
            printf("\n\n\ntx buff circle start\n\n\n\n");
            package_to_buff(&package_tx[index_read_tx], mbuf);
            index_read_tx++;
            if (index_read_tx >= PACK_CACHE)
                index_read_tx = 0;
            // pthread_mutex_lock(&mutex_readyNum_tx);
            // readyNum_tx--;
            // pthread_mutex_unlock(&mutex_readyNum_tx);
            // pthread_mutex_lock(&mutex_startNum_tx);
            // startNum_tx--;
            // pthread_mutex_unlock(&mutex_startNum_tx);
            pthread_mutex_lock(&mutex_buffisEmpty);
            buffisEmpty = 0;
            pthread_mutex_lock(&mutex_buffisEmpty);
            printf("tx buff circle end\n");
        }
    }
    pthread_mutex_destroy(&mutex_buffisEmpty);
    sem_post(&tx_buff_can_be_destroyed);
}

int package_to_buff(struct package_t *package, uint8_t *buff)
{
    // printf("package to buff start\n");
    uint8_t *buff_p = buff;
    int buff_length = 0;
    // printf("size of tbs:%ld\n", sizeof(package->tbs));

    uint8_t *tbs_p = (uint8_t *)(package->tbs);
    for (int i = 0; i < MAX_BEAM; i++)
    {
        *buff_p = *tbs_p;
        buff_p++;
        tbs_p++;
        buff_length++;
        // for (int j = sizeof(int) - 1; j >= 0; j--)
        // {
        //     printf("%d\n", j);
        //     *buff_p = package->tbs[i];
        // }
    }
    // printf("tbs done\n");

    uint8_t *cqi_p = (uint8_t *)(package->CQI_index);
    for (int i = 0; i < MAX_BEAM * sizeof(int); i++)
    {
        *buff_p = *cqi_p;
        buff_p++;
        cqi_p++;
        buff_length++;
    }
    // printf("cqi done\n");

    uint8_t *snr_p = (uint8_t *)(&(package->SNR));
    for (int i = 0; i < sizeof(float); i++)
    {
        *buff_p = *snr_p;
        buff_p++;
        snr_p++;
        buff_length++;
    }
    printf("snr done\n");

    uint8_t *y_p = (uint8_t *)(package->y);
    for (int i = 0;
         i < MAX_BEAM * CARRIER_NUM * SYMBOL_NUM *
                 sizeof(lapack_complex_float);
         i++)
    {
        *buff_p = *y_p;
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
        data_p = (uint8_t *)(package->data[j]);
        for (int i = 0; i < MAX_DATA_LEN_TX + CRC_LENGTH; i++)
        {
            tem = 0xFF;
            *buff_p = tem;
            // *buff_p = package->data[j][i];
            buff_p++;
            data_p++;
            // printf("buff_length:%d\n",buff_length);
            buff_length++;
        }
        // printf("data%d done\n", j);

    } // printf("data done\n");
    return buff_length;
}