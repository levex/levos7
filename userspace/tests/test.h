#ifndef __LEVOS_TEST_H
#define __LEVOS_TEST_H

extern int run_test();

#define CHECK_ERR(call, err) \
        printf("doing err \"" #call "\" expecting errno " #err "\n"); \
        if (!((rc = (call)) == -1 && errno == err)) { \
            printf("" #call " failed with rc %d and errno %d\n", rc, errno); \
            return 1; \
        }

#define CHECK(call, ret) \
        printf("doing \"" #call "\" expecting ret " #ret " \n"); \
        if (!((rc = (call)) == ret)) { \
            printf("" #call " failed with rc %d\n", rc); \
            return 1; \
        }
        

#endif
