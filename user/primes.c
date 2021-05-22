#include <kernel/types.h>
#include <user/user.h>

//通过递归来写子进程和子进程的子进程

void func(int *input, int num){
   if (num==1){
      printf("prime: %d\n", *input);
      return;
   }
   
   int p[2],i;
   int prime=*input;
   int temp;
   printf("prime: %d\n",prime);
   pipe(p);
   if (fork()==0){     //在新的pipe中构造数组,在管道中写入
      for (i=0;i<num;i++){     
         temp=*(input+i);
         write(p[1],(char *)(&temp),4);
      }
      exit(0);
   }
   close(p[1]);
   if (fork()==0){        //在新的pipe中构造数组,从管道中读出
      int counter=0;
      char buffer[4];
      while(read(p[0],buffer,4)!=0){
         temp=*((int *)buffer);
         if (temp%prime!=0){
            *input=temp;
            input+=1;
            counter++;
         }
      }
      func(input-counter,counter);
      exit(0);
   }
   wait(0);
   wait(0);

}



int main(){
   int a[34];
   for (int j=0;j<34;j++){
      a[j]=j+2;
   }
   func(a,34);
   exit(0);

}
