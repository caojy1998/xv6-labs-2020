#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  
  if (argc==1){
     printf("sleep error\n");
     exit(1);
  }
  char *p=argv[1];
  int i=atoi(p);
  //printf("%d\n",i);
  sleep(i);
  exit(0);
}
