#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

///#define XZL_DBG
// 11
int readline(char *buf)
{
    int ret = 0;
    while (read(0, buf, 1) == 1)
    {
        ret = 1;
        //printf("%c.",*buf);
        if (*buf == '\n')
        {
            break;
        }
        ++buf;
    }
    *buf = 0;
    return ret;
}

int main(int argc, char *argv[])
{
    char buf[MAXPATH];
    char *args[argc+1];
    memcpy(args, argv + 1, sizeof(argv[0]) * (argc - 1));
    args[argc - 1] = buf;
    args[argc] = 0;
    for (int i = 0; i <= argc; ++i)
    {
        ///printf("# %s ", args[i]); //0对应的应该是(null)
    }
    fprintf(2,"abc\n");   //这边用----和用\n显示的结果有一些不同

   
#ifdef XZL_DBG
    printf("Me %d is root argc %d\n", getpid(), argc);
    for (int i = 0; i <= argc; ++i)
    {
        printf("# %s\n", args[i]);
    }
#endif // XZL_DBG

    while (readline(buf))
    {
        int pid = fork();
        if (pid == 0)
        {
            // child
#ifdef XZL_DBG
            sleep(10);
            printf("Me %d is child argc %d\n", getpid(), argc);
            for (int i = 0; i <= argc; ++i)
            {
                printf("$ %s\n", args[i]);
            }
#endif // XZL_DBG
            exec(args[0], args);
            break; //exit(0)
        }

        // parent
#ifdef XZL_DBG
        printf("Me %d is parent pid %d\n", getpid(), pid);
#endif // XZL_DBG

        int xstate;
        int childpid = wait(&xstate);
        (void)childpid;

#ifdef XZL_DBG
        printf("Me %d is parent pid %d cpid %d xstate %d\n", getpid(), pid, childpid, xstate);
#endif // XZL_DBG
    }

    exit(0);
}
