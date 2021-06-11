#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000



struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];
int keys[NKEYS];
int nthread = 1;
pthread_mutex_t lock;            // declare a lock

double
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void 
insert(int key, int value, struct entry **p, struct entry *n)   //一个类似指针数组的东西，每次将插入的部分插入在头上面
{
  struct entry *e = malloc(sizeof(struct entry));
 
  e->key = key;
  e->value = value;
  e->next = n;
  
  *p = e;
}

static 
void put(int key, int value)       //首先根据mod5的余数分类,然后查找有没有重复出现的key,有的话覆盖掉，没有的话从头上覆盖掉
{
  int i = key % NBUCKET;
  
  

  // is the key already present?
  struct entry *e = 0;
 
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    pthread_mutex_lock(&lock);    //  作业
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock);  //  作业
  }
  
  
}

// 可能的情况就是连接到头部的时候出现了课上一样的错误，一个先改了值，还没连上去，另一个又开始了操作，等另一个操作完了，原来那个也就丢失了

static struct entry*
get(int key)                 //如果找不到就返回0
{
  int i = key % NBUCKET;


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }

  return e;
}

static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;
  
  pthread_mutex_t lock;            // declare a lock
  pthread_mutex_init(&lock, NULL); // initialize the lock  
  
  for (int i = 0; i < b; i++) {
    //pthread_mutex_lock(&lock);    //  作业
    put(keys[b*n + i], n);
    //pthread_mutex_unlock(&lock);  //  作业
    
  }
  
  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])
{
  pthread_mutex_init(&lock, NULL); // initialize the lock  锁定义和初始化在哪里是很重要的
  pthread_t *tha;
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);
  assert(NKEYS % nthread == 0);
  for (int i = 0; i < NKEYS; i++) {
    keys[i] = random();
  }

  //
  // first the puts
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
