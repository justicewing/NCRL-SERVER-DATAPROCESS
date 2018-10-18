

lapack_complex_float *SRS = (lapack_complex_float *)malloc(sizeof(lapack_complex_float) * LayerNum * CarrierNum);
for (int nl = 0; nl < layerNum; nl++)
{
    j = 0;
    int cycle = nl;
    for (int nc = 0; nc < CarrierNum; nc++)
    {
        k = nc * layerNum + nl;
        if (nc % layerNum == cycle)
        {
            SRS[k].real = sqrt(layerNum) * ZC[(j + cycle) % Nc].real;
            SRS[k].imag = sqrt(layerNum) * ZC[(j + cycle) % Nc].imag;
            j++;
        }
        else
            SRS[k] = c_zero;
    }
}