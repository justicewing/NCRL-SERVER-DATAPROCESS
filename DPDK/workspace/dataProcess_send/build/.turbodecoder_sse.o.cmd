cmd_turbodecoder_sse.o = icc -Wp,-MD,./.turbodecoder_sse.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/sherlockhsu/Github/DataProcess-Server-/DPDK/workspace/dataProcess_send/build/include -I/home/sherlockhsu/dpdk/x86_64-native-linuxapp-icc/include -include /home/sherlockhsu/dpdk/x86_64-native-linuxapp-icc/include/rte_config.h -g -Wall -I . -msse4.1 -O3 -mavx  -I/home/sherlockhsu/Github/DataProcess-Server-/DPDK/workspace/dataProcess_send -O3  -Wall -w2 -diag-disable 271 -diag-warning 1478 -diag-disable 13368 -diag-disable 15527 -diag-disable 188 -diag-disable 11074 -diag-disable 11076    -o turbodecoder_sse.o -c /home/sherlockhsu/Github/DataProcess-Server-/DPDK/workspace/dataProcess_send/turbodecoder_sse.c 
