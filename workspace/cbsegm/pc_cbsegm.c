#include <string.h>

#include "pc_cbsegm.h"

int seu5g_pc_cbsegm(seu5g_pc_cbsegm_t *s, uint32_t tbs, uint32_t I_seg)
{
    int ret;
    if (tbs == 0)
    {
        bzero(s, sizeof(seu5g_pc_cbsegm_t));
        ret = SEU5G_SUCCESS;
    }
    else if(tbs)
}