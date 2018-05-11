OBJS = main.o
DEST = main
LIBS = -lm -lpthread -lmkl_rt -mavx -fopenmp
CPPFLAGS = -g -Wall -I . -O3 -mavx

main: ${OBJS}
	gcc -o ${DEST} ${OBJS} ${LIBS}
clean:
	rm -f ${DEST} ${OBJS}
