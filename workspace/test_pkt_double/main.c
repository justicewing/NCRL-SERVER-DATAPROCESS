#include <stdio.h>
#include <stdlib.h>

#define ARRAYD_SIZE 128
#define ARRAYC_SIZE (ARRAYD_SIZE * sizeof(double) / sizeof(unsigned char))

int const size = 10;
const int paraNum_tx = 2;
const int MaxBeam = 2;


struct ts
{
    int *data;
};

int main()
{
    int data_len_tx[paraNum_tx][MaxBeam];
    struct ts tss;
    tss.data = (int *)malloc(sizeof(int) * 100);
    printf("size of ts: %ld\n\n", sizeof(tss));
    double *arrayD;
    unsigned char *arrayC;
    double *arrayD_r;
    unsigned char arrayC_r[ARRAYC_SIZE];

    arrayD = (double *)malloc(sizeof(double) * ARRAYD_SIZE);

    printf("\n");
    for (int i = 0; i < ARRAYD_SIZE; i++)
        arrayD[i] = i;
    arrayC = (unsigned char *)arrayD;

    for (int i = 0; i < ARRAYD_SIZE; i++)
    {
        for (int j = 0; j < sizeof(double) / sizeof(unsigned char); j++)
            printf("%02X, ", arrayC[i]);
        printf("\n");
    }

    for (int i; i < ARRAYC_SIZE; i++)
        arrayC_r[i] = arrayC[i];
    arrayD_r = (double *)arrayC_r;

    for (int i = 0; i < ARRAYD_SIZE; i++)
        printf("%f\n", arrayD_r[i]);

    free(arrayD);

    unsigned char *arrayR =
        (unsigned char *)malloc(sizeof(unsigned char) *
                                ARRAYC_SIZE);
    for (int i = 0; i < ARRAYC_SIZE; i++)
        arrayR[i] = 0x0f;

    printf("\n");
    for (int i = 0; i < ARRAYD_SIZE; i++)
    {
        for (int j = 0; j < sizeof(double) / sizeof(unsigned char); j++)
            printf("%02X, ", arrayR[i]);
        printf("\n");
    }

    double *arrayR_D = (double *)arrayR;
    for (int i = 0; i < ARRAYD_SIZE; i++)
        printf("%f\n", arrayR_D[i]);
    free(arrayR);
}