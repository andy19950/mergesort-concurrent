#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "threadpool.h"
#include "list.h"

#define USAGE "usage: ./sort [thread_count] [file_name]\n"
#define BUF_SIZ 512

static llist_t *tmp_list;
static llist_t *the_list = NULL;
static uint32_t now = 0;
static int thread_count = 0, data_count = 0, max_cut = 0, cut_thread_count = 0;
static tpool_t *pool = NULL;

llist_t *merge_list(llist_t *a, llist_t *b)
{
    llist_t *_list = list_new();
    node_t *current = NULL;
    while (a->size && b->size) {
        int cmp = strcmp(a->head->data, b->head->data);
        llist_t *small = (llist_t *)
                         ((intptr_t) a * (cmp <= 0) +
                          (intptr_t) b * (cmp > 0));
        if (current) {
            current->next = small->head;
            current = current->next;
        } else {
            _list->head = small->head;
            current = _list->head;
        }
        small->head = small->head->next;
        --small->size;
        ++_list->size;
        current->next = NULL;
    }

    llist_t *remaining = (llist_t *) ((intptr_t) a * (a->size > 0) +
                                      (intptr_t) b * (b->size > 0));
    if (current) current->next = remaining->head;
    _list->size += remaining->size;
    free(a);
    free(b);
    return _list;
}

llist_t *merge_sort(llist_t *list)
{
    if (list->size < 2)
        return list;
    int mid = list->size >> 1;
    llist_t *left = list;
    llist_t *right = list_new();
    right->head = list_nth(list, mid);
    right->size = list->size - mid;
    list_nth(list, mid - 1)->next = NULL;
    left->size = mid;
    return merge_list(merge_sort(left), merge_sort(right));
}

void merge(void *data)
{
    llist_t *_list = (llist_t *) data;
    if (_list->size < (uint32_t) data_count) {
        llist_t *_t = tmp_list;
        if (!_t) {
            tmp_list = _list;
        } else {
            tmp_list = NULL;
            task_t *_task = (task_t *) malloc(sizeof(task_t));
            _task->func = merge;
            _task->arg = merge_list(_list, _t);
            tqueue_push(pool->queue + now++%thread_count, _task);
        }
    } else {
        the_list = _list;
        task_t *_task = (task_t *) malloc(sizeof(task_t));
        _task->func = NULL;
        for(int i=0; i<pool->count; i++)
            tqueue_push(pool->queue + i, _task);
        list_print(_list);
    }
}

void cut_func(void *data)
{
    llist_t *list = (llist_t *) data;
    int cut_count = thread_count;
    if (list->size > 1 && cut_count < max_cut) {
        ++cut_thread_count;

        /* cut list */
        int mid = list->size >> 1;
        llist_t *_list = list_new();
        _list->head = list_nth(list, mid);
        _list->size = list->size - mid;
        list_nth(list, mid - 1)->next = NULL;
        list->size = mid;

        /* create new task: left */
        task_t *_task = (task_t *) malloc(sizeof(task_t));
        _task->func = cut_func;
        _task->arg = list;
        tqueue_push(pool->queue + now++%thread_count, _task);

        /* create new task: right */
        _task = (task_t *) malloc(sizeof(task_t));
        _task->func = cut_func;
        _task->arg = _list;
        tqueue_push(pool->queue + now++%thread_count, _task);


    } else {
        merge(merge_sort(list));
    }
}

static void *task_run(void *data)
{
    tqueue_t *_queue = (tqueue_t *) data;
    while (1) {
        task_t *_task = tqueue_pop(_queue);
        if (_task) {
            if (!_task->func) {
                tqueue_push(_queue, _task);
                break;
            } else {
                _task->func(_task->arg);
                free(_task);
            }
        }
    }
    pthread_exit(NULL);
}

void data_correctness()
{
    FILE *result, *answer;
    result = fopen("output.txt", "r");
    answer = fopen("words.txt", "r");
    char a[BUF_SIZ], b[BUF_SIZ];
    while(fgets(a, sizeof(a), answer) != NULL) {
        fgets(b, sizeof(b), result);
        assert(!strcmp(a, b) && "ERROR : Some name is not in the linked-list!!");
    }
    fclose(result);
    fclose(answer);
}

static double diff_in_second(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec-t1.tv_nsec < 0) {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec  = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec + diff.tv_nsec / 1000000000.0);
}


int main(int argc, char const *argv[])
{
    char *input_file, line[BUF_SIZ];
    FILE *fp;
    struct timespec start, end;
    double cpu_time;

    if (argc < 3) {
        printf(USAGE);
        return -1;
    }
    thread_count = atoi(argv[1]);
    input_file = argv[2];
    fp = fopen(input_file, "r");
    srand(time(NULL));
    max_cut = thread_count * (thread_count <= data_count) +
              data_count * (thread_count > data_count) - 1;

    /* Read data */
    the_list = list_new();
    while(fgets(line, sizeof(line), fp) != NULL) {
        line[strlen(line)-1] = '\0';
        list_add(the_list, line);
    }

    /* initialize tasks inside thread pool */
    tmp_list = NULL;
    pool = (tpool_t *) malloc(sizeof(tpool_t));
    tpool_init(pool, thread_count, task_run);
    clock_gettime(CLOCK_REALTIME, &start);

    /* launch the first task */
    task_t *_task = (task_t *) malloc(sizeof(task_t));
    _task->func = cut_func;
    _task->arg = the_list;
    tqueue_push(pool->queue + now++%thread_count, _task);

    /* release thread pool */
    tpool_free(pool);
    clock_gettime(CLOCK_REALTIME, &end);
    cpu_time = diff_in_second(start, end);
    printf("%d %.4lf\n", thread_count, cpu_time);
    fclose(fp);

    /* data correctness */
    data_correctness();
    return 0;
}
