cmd_QAM_NDemod.o = gcc -Wp,-MD,./.QAM_NDemod.o.d.tmp  -m64 -pthread  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_FSGSBASE -DRTE_MACHINE_CPUFLAG_F16C -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/sherlockhsu/Github/DataProcess-Server-/DPDK/workspace/dataProcess_send/build/include -I/home/sherlockhsu/dpdk/x86_64-native-linuxapp-gcc/include -include /home/sherlockhsu/dpdk/x86_64-native-linuxapp-gcc/include/rte_config.h -g -Wall -I . -msse4.1 -O3 -mavx  -I/home/sherlockhsu/Github/DataProcess-Server-/DPDK/workspace/dataProcess_send -O3  -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings    -o QAM_NDemod.o -c /home/sherlockhsu/Github/DataProcess-Server-/DPDK/workspace/dataProcess_send/QAM_NDemod.c 
