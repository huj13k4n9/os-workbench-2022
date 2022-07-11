#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <dirent.h>
#include <ctype.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define MAX_CHILD_NUM 512

struct process_node {
    char name[128];
    pid_t pid;
    pid_t ppid;
    uint32_t children_num;
    struct process_node *next;
    struct process_node *parent;
    struct process_node *children[MAX_CHILD_NUM];
};

static struct process_node *pstree_head = NULL;

void print_help(void) {
    printf("A program to print current process tree.\n"
           "Usage: pstree [OPTION...]\n\n"
           "  -h, --help             give this help list\n"
           "  -n, --numeric-sort     sort the output by PID\n"
           "  -p, --show-pids        show process PID number\n"
           "  -V, --version          print program version\n");
}

void init_head_node() {
    pstree_head = (struct process_node *) malloc(sizeof(struct process_node));
    pstree_head->pid = 0;
    pstree_head->ppid = -1;
    pstree_head->next = NULL;
    pstree_head->parent = NULL;
    pstree_head->children_num = 0;
    strcpy(pstree_head->name, "init");
    memset(pstree_head->children, 0, sizeof(struct process_node *) * MAX_CHILD_NUM);
}

struct process_node * get_node_by_pid(pid_t pid) {
    struct process_node *tmp;
    for (tmp = pstree_head; tmp != NULL; tmp = tmp->next) {
        if (tmp->pid == pid)  return tmp;
    }
    return NULL;
}

void remove_parentheses(char *name) {
    int length = strlen(name);
    for (int i = 0; i < length - 1; i++) {
        name[i] = name[i + 1];
    }
    name[length - 2] = '\000';
}

void read_processes_info(void) {
    DIR *proc = opendir("/proc");
    struct dirent *entry;
    char *stat_path, *stat_buf;
    int stat_fd;
    struct process_node *tail, *tmp;

    if (proc == NULL) {
        perror("opendir(\"/proc\")");
        goto error;
    }

    char state;
    bool is_first_node = true;
    while ((entry = readdir(proc))) {
        // Exclude the non-PID directories
        if (!isdigit(*entry->d_name))  continue;

        // Fill the /proc/[pid]/stat path
        stat_path = (char *) malloc(64 * sizeof(char));
        if (snprintf(stat_path, 64, "/proc/%s/stat", entry->d_name) < 0)  goto error;

        // Read /proc/[pid]/stat data
        stat_fd = open(stat_path, O_RDONLY);
        stat_buf = (char *) malloc(1024 * sizeof(char));
        read(stat_fd, stat_buf, 1024);

        // Init linked list
        tmp = (struct process_node *) malloc(sizeof(struct process_node));
        memset(tmp->children, 0, sizeof(struct process_node *) * MAX_CHILD_NUM);
        sscanf(stat_buf, "%d %s %c %d ", &(tmp->pid), tmp->name, &state, &(tmp->ppid));
        remove_parentheses(tmp->name);
        tmp->next = NULL;
        tmp->children_num = 0;


        if (is_first_node) {
            pstree_head->next = tmp;
            is_first_node = false;
        } else {
            tail->next = tmp;
        }
        tail = tmp;
    }

    return;
error:
    exit(EXIT_FAILURE);
}

void build_process_tree(void) {
    struct process_node *node, *parent_node;
    for (node = pstree_head->next; node != NULL; node = node->next) {
        // Find the parent_node of every process (except process 0)
        parent_node = get_node_by_pid(node->ppid);
        if (parent_node != NULL) {
            // Found
            node->parent = parent_node;
            parent_node->children[parent_node->children_num++] = node;
        } else {
            // Not found, error
            perror("Cannot find parent_node");
            exit(EXIT_FAILURE);
        }
    }
}

void print_process_tree(struct process_node *head, bool show_pids, bool numeric_sort, int level) {
    int i;
    for (i = 0; i < level; i++)  printf("    ");
    printf("└─ %s\n", head->name);
    for (i = 0; i < head->children_num; i++) {
        print_process_tree(head->children[i], show_pids, numeric_sort, level + 1);
    }
}

int main(int argc, char *argv[]) {
    int c, opt_index = 0;
    bool show_pids = false, numeric_sort = false;

    while(true) {
        static struct option opts[] = {
            {.name = "show-pids", .has_arg = no_argument, .flag = 0, .val = 'p'},
            {.name = "numeric-sort", .has_arg = no_argument, .flag = 0, .val = 'n'},
            {.name = "version", .has_arg = no_argument, .flag = 0, .val = 'V'},
            {.name = "help", .has_arg = no_argument, .flag = 0, .val = 'h'},
        };
        c = getopt_long(argc, argv, "pnVh", opts, &opt_index);
        if (c == -1)  break;

        switch (c) {
            case 'p':
                printf("Argument -p/--show-pids is activated.\n");
                show_pids = true;
                break;
            case 'n':
                printf("Argument -n/--numeric-sort is activated.\n");
                numeric_sort = true;
                break;
            case 'V':
                printf("pstree -- version 0.01\n");
                break;
            case 'h':
                print_help();
                break;
            case '?':
            default:
                printf("Use option -h/--help to get more information.\n");
                exit(EXIT_SUCCESS);
        }
    }

    init_head_node();
    read_processes_info();
    build_process_tree();
    print_process_tree(pstree_head, show_pids, numeric_sort, 0);
    exit(EXIT_SUCCESS);
}