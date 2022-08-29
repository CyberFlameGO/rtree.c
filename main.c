#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "rtree.h"

int main() {
    // new tree
    struct rtree *tr = rtree_new(sizeof(int), 3);

    // insert 10,000,000 rectangles
    int N = 10000000;
    clock_t start = clock();
    for (int i=0;i<N;i++) {
        double values[6];
        values[0] = ((double)rand()/(double)RAND_MAX);
        values[1] = ((double)rand()/(double)RAND_MAX);
        values[2] = ((double)rand()/(double)RAND_MAX);
        values[3] = values[0] + 0.1;
        values[4] = values[1] + 0.1;
        values[5] = values[2] + 0.1;
        if (!rtree_insert(tr, values, &i)) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
    }
    clock_t end = clock();
    printf("inserted %zu objects in %.0f ms\n", rtree_count(tr),
        ((float)(end-start))/CLOCKS_PER_SEC*1000);

    // free tree
    start = clock();
    rtree_free(tr);
    end = clock();
    printf("freed tree in %.0f ms\n",
        ((float)(end-start))/CLOCKS_PER_SEC*1000);
}
