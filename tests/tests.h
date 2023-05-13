#ifndef TESTS_H
#define TESTS_H

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include "../rtree.h"

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wcompound-token-split-by-macro"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wpedantic"
#endif


// private rtree functions
bool rtree_check(struct rtree *tr);
void rtree_write_svg(struct rtree *tr, const char *path);

int64_t crand(void) {
    uint64_t seed = 0;
    FILE *urandom = fopen("/dev/urandom", "r");
    assert(urandom);
    assert(fread(&seed, sizeof(uint64_t), 1, urandom));
    fclose(urandom);
    return (int64_t)(seed>>1);
}

void seedrand(void) {
    srand(crand());
}

static int64_t seed = 0;

#define do_test0(name, trand) { \
    if (argc < 2 || strstr(#name, argv[1])) { \
        if ((trand)) { \
            seed = getenv("SEED")?atoi(getenv("SEED")):crand(); \
            printf("SEED=%" PRId64 "\n", seed); \
            srand(seed); \
        } else { \
            seedrand(); \
        } \
        printf("%s\n", #name); \
        init_test_allocator(false); \
        name(); \
        cleanup(); \
        cleanup_test_allocator(); \
    } \
}

#define do_test(name) do_test0(name, 0)
#define do_test_rand(name) do_test0(name, 1)

#define do_chaos_test(name) { \
    if (argc < 2 || strstr(#name, argv[1])) { \
        printf("%s\n", #name); \
        seedrand(); \
        init_test_allocator(true); \
        name(); \
        cleanup(); \
        cleanup_test_allocator(); \
    } \
}

void shuffle(void *array, size_t numels, size_t elsize) {
    if (numels < 2) return;
    char tmp[elsize];
    char *arr = array;
    for (size_t i = 0; i < numels - 1; i++) {
        int j = i + rand() / (RAND_MAX / (numels - i) + 1);
        memcpy(tmp, arr + j * elsize, elsize);
        memcpy(arr + j * elsize, arr + i * elsize, elsize);
        memcpy(arr + i * elsize, tmp, elsize);
    }
}

void cleanup(void) {
}

atomic_int total_allocs = 0;
atomic_int total_mem = 0;

double now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*1e9 + now.tv_nsec) / 1e9;
}

static bool rand_alloc_fail = false;
// 1 in 10 chance malloc or realloc will fail.
static int rand_alloc_fail_odds = 3; 

static void *xmalloc(size_t size) {
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    void *mem = malloc(sizeof(uint64_t)+size);
    assert(mem);
    *(uint64_t*)mem = size;
    atomic_fetch_add(&total_allocs, 1);
    atomic_fetch_add(&total_mem, (int)size);
    return (char*)mem+sizeof(uint64_t);
}

static void xfree(void *ptr) {
    if (ptr) {
        atomic_fetch_sub(&total_mem,
            (int)(*(uint64_t*)((char*)ptr-sizeof(uint64_t))));
        atomic_fetch_sub(&total_allocs, 1);
        free((char*)ptr-sizeof(uint64_t));
    }
}

static void *(*__malloc)(size_t) = NULL;
static void (*__free)(void *) = NULL;

void init_test_allocator(bool random_failures) {
    rand_alloc_fail = random_failures;
    __malloc = xmalloc;
    __free = xfree;
}

void cleanup_test_allocator(void) {
    if (atomic_load(&total_allocs) > 0 || atomic_load(&total_mem) > 0) {
        fprintf(stderr, "test failed: %d unfreed allocations, %d bytes\n",
            atomic_load(&total_allocs), atomic_load(&total_mem));
        exit(1);
    }
    __malloc = NULL;
    __free = NULL;
}

double rand_double() {
    return (double)rand() / ((double)RAND_MAX+1);
}

struct rect {
    double min[2];
    double max[2];
};

void fill_rand_rect(double coords[]) {
    coords[0] = rand_double()*360-180;
    coords[1] = rand_double()*180-90;
    coords[2] = coords[0]+(rand_double()*2);
    coords[3] = coords[1]+(rand_double()*2);
}

struct rect rand_rect() {
    struct rect rect;
    fill_rand_rect(&rect.min[0]);
    return rect;
}

// // struct btree *btree_new_for_test(size_t elsize, size_t max_items,
// //     int (*compare)(const void *a, const void *b, void *udata),
// //     void *udata)
// // {
// //     return btree_new_with_allocator(__malloc, NULL, __free, elsize, max_items, 
// //         compare, udata);
// // }

// struct btree *btree_new_for_test(size_t elsize, size_t degree,
//     int (*compare)(const void *a, const void *b, void *udata),
//     void *udata)
// {
//     return btree_new_with_allocator(__malloc, NULL, __free, elsize, 
//         degree, compare, udata);
// }


struct find_one_context {
    void *target;
    bool found;
    int(*compare)(const void *a, const void *b);
};

bool find_one_iter(const double *min, const double *max, const void *data,
    void *udata)
{
    (void)min; (void)max;
    struct find_one_context *ctx = udata;
    if ((ctx->compare && ctx->compare(data, ctx->target) == 0) ||
        data == ctx->target)
    {
        assert(!ctx->found);
        ctx->found = true;
    }
    return true;
}

bool find_one(struct rtree *tr, const double min[], const double max[], 
    void *data, int(*compare)(const void *a, const void *b))
{
    struct find_one_context ctx = { .target = data, .compare = compare };
    rtree_search(tr, min, max, find_one_iter, &ctx);
    return ctx.found;
}

char *commaize(unsigned int n) {
    char s1[64];
    char *s2 = malloc(64);
    assert(s2);
    memset(s2, 0, sizeof(64));
    snprintf(s1, sizeof(s1), "%d", n);
    int i = strlen(s1)-1; 
    int j = 0;
	while (i >= 0) {
		if (j%3 == 0 && j != 0) {
            memmove(s2+1, s2, strlen(s2)+1);
            s2[0] = ',';
		}
        memmove(s2+1, s2, strlen(s2)+1);
		s2[0] = s1[i];
        i--;
        j++;
	}
	return s2;
}

char *rand_key(int nchars) {
    char *key = xmalloc(nchars+1);
    for (int i = 0 ; i < nchars; i++) {
        key[i] = (rand()%26)+'a';
    }
    key[nchars] = '\0';
    return key;
}

static void *oom_ptr = (void*)(uintptr_t)(intptr_t)-1;

void *rtree_set(struct rtree *tr, const double *min, const double *max, 
    void *data)
{
    void *prev = NULL;
    size_t n = rtree_count(tr);
    if (find_one(tr, min, max, data, NULL)) {
        prev = data;
        size_t n = rtree_count(tr);
        if (!rtree_delete(tr, min, max, data)) return oom_ptr;
        assert(rtree_count(tr) == n-1);
        n--;
    }
    if (!rtree_insert(tr, min, max, data)) return oom_ptr;
    assert(rtree_count(tr) == n+1);
    
    return prev;
}

// rsleep randomly sleeps between min_secs and max_secs
void rsleep(double min_secs, double max_secs) {
    double duration = max_secs - min_secs;
    double start = now();
    while (now()-start < duration) {
        usleep(10000); // sleep for ten milliseconds
    }
}

#endif // TESTS_H


