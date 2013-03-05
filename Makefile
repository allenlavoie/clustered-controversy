CFLAGS = --std=c99 -O3 -Wall
#CFLAGS = --std=c99 -g -Wall
LIBS = -lpthread -lm
COMMON_OBJS = score_thread.o compute_scores.o read_mmap.o queue.o

all: similarity make_mmap cc_mmap
similarity: $(COMMON_OBJS) similarity.o
	gcc $(CFLAGS) $(COMMON_OBJS) similarity.o $(LIBS) -o similarity
make_mmap: $(COMMON_OBJS) make_mmap.o
	gcc $(CFLAGS) $(COMMON_OBJS) make_mmap.o $(LIBS) -o make_mmap
cc_mmap: $(COMMON_OBJS) cc_mmap.o
	gcc $(CFLAGS) $(COMMON_OBJS) cc_mmap.o $(LIBS) -o cc_mmap
clean:
	rm *.o
