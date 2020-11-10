#ifndef __VPTree
#define __VPTree

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef vpt_t
#define vpt_t void*
#endif

#define DEBUG 0
#if DEBUG
#include <assert.h>
#include <stdio.h>
#define LOG(format, message) \
    printf(format, message); \
    fflush(stdout);

#define LOGs(message)        \
    printf("%s\n", message); \
    fflush(stdout);
#endif
#ifndef LOG
#define LOG(format, message) ;
#define LOGs(format) ;
#endif

#define VPT_BUILD_SMALL_THRESHOLD 250
#define VPT_BUILD_LIST_THRESHOLD 100
#define VPT_MAX_HEIGHT 100

/**********************/
/* Struct Definitions */
/**********************/

struct VPNode;
typedef struct VPNode VPNode;
union NodeUnion;
typedef union NodeUnion NodeUnion;
struct VPBranch;
typedef struct VPBranch VPBranch;
struct PList;
typedef struct PList PList;

// This is a labeled union, containing either a branch in the tree, or a point list.
struct VPNode {  // 40 with vpt_t = void*
    char ulabel;
    union NodeUnion {
        struct VPBranch {
            vpt_t item;
            double radius;
            VPNode* left;
            VPNode* right;
        } branch;
        struct PList {
            vpt_t* items;
            size_t size;
            size_t capacity;
        } pointlist;
    } u;
};

struct VPTree {
    VPNode* root;
    size_t size;
    double (*dist_fn)(const vpt_t first, const vpt_t second);
};
typedef struct VPTree VPTree;

struct VPEntry {
    vpt_t item;
    double distance;
};
typedef struct VPEntry VPEntry;

struct VPBuildStackFrame {
    VPNode* parent;
    VPEntry* children;
    size_t num_children;
};
typedef struct VPBuildStackFrame VPBuildStackFrame;

/***********************************/
/* Sort (Necessary for tree build) */
/***********************************/

void shellsort(VPEntry* arr, size_t n) {
    size_t interval, i, j;
    VPEntry temp;
    for (interval = n / 2; interval > 0; interval /= 2) {
        for (i = interval; i < n; i += 1) {
            temp = arr[i];
            for (j = i; j >= interval && arr[j - interval].distance > temp.distance; j -= interval) {
                arr[j] = arr[j - interval];
            }
            arr[j] = temp;
        }
    }
}

void __VPT_small_build(VPTree* vpt, vpt_t* data, size_t num_items) {}

/****************/
/* Tree Methods */
/****************/

// TODO find a better way to do this
VPNode* alloc_VPNode() {
    LOGs("Allocated node.");
    return (VPNode*)malloc(sizeof(VPNode));
}

void VPT_build(VPTree* vpt, vpt_t* data, size_t num_items,
               double (*dist_fn)(const vpt_t first, const vpt_t second)) {
    vpt->dist_fn = dist_fn;
    vpt->size = num_items;

    if (num_items < VPT_BUILD_SMALL_THRESHOLD) {
        LOG("Building small tree of size %lu.\n", num_items)
        __VPT_small_build(vpt, data, num_items);
        return;
    }
    LOG("Building large tree of size %lu.\n", num_items)

    size_t i;

    // Node to partition into left and right lists
    VPNode* newnode;
    VPEntry *right_children, *left_children;
    size_t right_num_children, left_num_children;

    // Hold information about the nodes that still need to be created, both on the left and right
    VPBuildStackFrame popped;
    VPBuildStackFrame leftstack[VPT_MAX_HEIGHT];
    VPBuildStackFrame rightstack[VPT_MAX_HEIGHT];
    size_t left_stacksize = 0, right_stacksize = 0;

    // Pop the first item off the list of points. It will become the root.
    vpt_t sort_by = *data;
    data += 1;
    size_t num_entries = (num_items - 1);
    LOGs("Popped first item off list.");

    // Copy the rest of the data into an array so it can be sorted and resorted as the tree is built
    VPEntry* build_buffer;
    VPEntry* entry_list = build_buffer = (VPEntry*)malloc(num_entries * sizeof(VPEntry));
    for (i = 0; i < num_entries; i++) {
        entry_list[i].item = data[i];
    }

    // Split the list in half using the root as a vantage point.
    // Sort the list by distance to the root
    for (i = 0; i < num_entries; i++)
        entry_list[i].distance = dist_fn(entry_list[i].item, sort_by);
    shellsort(entry_list, num_entries);
    LOGs("Entry list copied and sorted.");

    // Find median item and record position, splitting the list in (roughly) half.
    // Look backward for identical elements, and go forward until you're free of them.
    // This way, the right list is strictly greater than the left list.
    right_num_children = num_entries / 2;
    right_children = entry_list + (num_entries - right_num_children);
    while (right_children->distance == (right_children - 1)->distance) {
        --right_children;
        ++right_num_children;
    }
    left_num_children = num_entries - right_num_children;
    left_children = entry_list;

    // Set the node.
    newnode = alloc_VPNode();
    newnode->ulabel = 'b';
    newnode->u.branch.item = sort_by;
    newnode->u.branch.radius = (right_children - 1)->distance;
    vpt->root = newnode;

    // Push onto the stack the work that needs to be done to create the left and right of the root.
    leftstack[0].children = entry_list;
    leftstack[0].num_children = left_num_children;
    leftstack[0].parent = newnode;
    left_stacksize++;
    rightstack[0].children = right_children;
    rightstack[0].num_children = right_num_children;
    rightstack[0].parent = newnode;
    right_stacksize++;

    LOGs("Finished initializing the stack.");

    // Now build the rest of the tree off the root. Push the information for the nodes
    // that still need to be created onto the stacks, and keep taking from them until
    // we're done.
    while (left_stacksize || right_stacksize) {
        if (left_stacksize) {
            popped = leftstack[--left_stacksize];
            LOG("Popped %lu off left stack\n", popped.num_children);
            /*******************/
            /* Build left node */
            /*******************/

            // Base case, build list and don't push.
            // This list is exactly sized, but can be realloced later.
            if (popped.num_children < VPT_BUILD_LIST_THRESHOLD) {
                // Find a smarter way to allocate memory than by peppering the heap
                // with like 5 million individually allocated nodes
                newnode = alloc_VPNode();
                newnode->ulabel = 'l';
                newnode->u.pointlist.size = newnode->u.pointlist.capacity = popped.num_children;
                newnode->u.pointlist.items = (vpt_t*)malloc(popped.num_children * sizeof(vpt_t));
                for (i = 0; i < popped.num_children; i++) {
                    newnode->u.pointlist.items[i] = popped.children[i].item;
                }
                popped.parent->u.branch.left = newnode;
                LOGs("Created leaf.");
            }

            // Inductive case, build node and push more information
            else {
                newnode = alloc_VPNode();

                // Pop the first child off the list and into the new node
                sort_by = popped.children[0].item;
                entry_list = (popped.children + 1);
                num_entries = (popped.num_children - 1);

                // Sort the entries by the popped node
                for (i = 0; i < num_entries; i++)
                    entry_list[i].distance =
                        dist_fn(sort_by, entry_list[i].item);
                shellsort(entry_list, num_entries);

                // Split the list of entries by the median.
                right_num_children = num_entries / 2;
                right_children = entry_list + (num_entries - right_num_children);
                while (right_children->distance == (right_children - 1)->distance) {
                    --right_children;
                    ++right_num_children;
                }
                left_num_children = num_entries - right_num_children;
                left_children = entry_list;
                LOG("Number of left children: %lu\n", left_num_children);
                LOG("Number of right children: %lu\n", right_num_children);

                // Set the information in the node. The node's radius is the distance of the
                // final element of the left list, such that left <= radius < right.
                newnode->ulabel = 'b';
                newnode->u.branch.item = sort_by;
                newnode->u.branch.radius = (right_children - 1)->distance;

                // Connect the node to its parent
                popped.parent->u.branch.left = newnode;

                // Push the lists and the relevant information for building the next left and
                // right of this node onto the stack
                leftstack[left_stacksize].children = entry_list;
                leftstack[left_stacksize].num_children = left_num_children;
                leftstack[left_stacksize].parent = newnode;
                ++left_stacksize;
                rightstack[right_stacksize].children = right_children;
                rightstack[right_stacksize].num_children = right_num_children;
                rightstack[right_stacksize].parent = newnode;
                ++right_stacksize;
                LOGs("Created branch.");
            }
        } else {
            popped = rightstack[--right_stacksize];
            LOG("Popped %lu off right stack\n", popped.num_children);
            /********************/
            /* Build right node */
            /********************/

            if (popped.num_children < VPT_BUILD_LIST_THRESHOLD) {
                newnode = alloc_VPNode();
                newnode->ulabel = 'l';
                newnode->u.pointlist.size = popped.num_children;
                newnode->u.pointlist.capacity = popped.num_children;
                newnode->u.pointlist.items = (vpt_t*)malloc(popped.num_children * sizeof(vpt_t));
                for (i = 0; i < popped.num_children; i++) {
                    newnode->u.pointlist.items[i] = popped.children[i].item;
                }
                popped.parent->u.branch.right = newnode;
                LOGs("Created leaf.");
            }

            // Inductive case, build node and push more information
            else {
                newnode = alloc_VPNode();

                // Pop the first child off the list and into the new node
                sort_by = popped.children[0].item;
                entry_list = (popped.children + 1);
                num_entries = (popped.num_children - 1);

                // Sort the entries by the popped node
                for (i = 0; i < num_entries; i++)
                    entry_list[i].distance =
                        dist_fn(sort_by, entry_list[i].item);
                shellsort(entry_list, num_entries);

                // Split the list of entries by the median.
                right_num_children = num_entries / 2;
                right_children = entry_list + (num_entries - right_num_children);
                while (right_children->distance == (right_children - 1)->distance) {
                    --right_children;
                    ++right_num_children;
                }
                left_num_children = num_entries - right_num_children;
                left_children = entry_list;
                LOG("Number of left children: %lu\n", left_num_children);
                LOG("Number of right children: %lu\n", right_num_children);

                newnode->ulabel = 'b';
                newnode->u.branch.item = sort_by;
                newnode->u.branch.radius = (right_children - 1)->distance;

                // Connect the node to its parent
                popped.parent->u.branch.right = newnode;

                // Push the lists and the relevant information for building the next left and
                // right of this node onto the stack
                leftstack[left_stacksize].children = entry_list;
                leftstack[left_stacksize].num_children = left_num_children;
                leftstack[left_stacksize].parent = newnode;
                ++left_stacksize;
                rightstack[right_stacksize].children = right_children;
                rightstack[right_stacksize].num_children = right_num_children;
                rightstack[right_stacksize].parent = newnode;
                ++right_stacksize;
                LOGs("Created branch.");
            }
        }
    }
    free(build_buffer);
}

bool VPT_build_empty(VPTree* vpt, double (*dist_fn)(const vpt_t first, const vpt_t second));

void VPT_destroy(VPTree* vpt) {
    VPNode* toTraverse[VPT_MAX_HEIGHT];
    size_t traverse_stack_size = 1;

    VPNode* popped;
    toTraverse[0] = vpt->root;
    while (traverse_stack_size) {
        popped = toTraverse[--traverse_stack_size];
        if (popped->ulabel == 'l') {  // List
            LOGs("Freeing list buffer.");
            free(popped->u.pointlist.items);
        } else {
            LOGs("Pushing left and right onto traverse stack.");
            toTraverse[traverse_stack_size++] = popped->u.branch.right;
            toTraverse[traverse_stack_size++] = popped->u.branch.left;
        }
        LOGs("Freeing node.");
        free(popped);
    }
    LOGs("Tree destruction complete.");
}

vpt_t* VPT_teardown(VPTree* vpt) {
    size_t list_capacity = vpt->size;
    size_t list_size = 0;
    vpt_t* all_items = (vpt_t*)malloc(list_capacity * sizeof(vpt_t));
    LOG("Making tree of size %lu\n", list_capacity);

    VPNode* toTraverse[VPT_MAX_HEIGHT];
    size_t traverse_stack_size = 1;

    VPNode* popped;
    toTraverse[0] = vpt->root;
    while (traverse_stack_size) {
        popped = toTraverse[--traverse_stack_size];

        if (popped->ulabel == 'l') {  // List
            LOGs("Retrieving items from list.");
            for (size_t i = 0; i < popped->u.pointlist.size; i++) {
                all_items[list_size++] = popped->u.pointlist.items[i];
            }
            LOGs("Freeing list buffer.");
            free(popped->u.pointlist.items);
        } else {
            LOGs("Retrieving item from branch.");
            all_items[list_size++] = popped->u.branch.item;
            LOGs("Pushing left and right onto traverse stack.");
            toTraverse[traverse_stack_size++] = popped->u.branch.right;
            toTraverse[traverse_stack_size++] = popped->u.branch.left;
        }
        LOGs("Freeing node.");
        free(popped);
    }

    LOGs("Tree disassembly complete.");
    return all_items;
}

void VPT_rebuild(VPTree* vpt) {
    vpt_t* items = VPT_teardown(vpt);
    VPT_build(vpt, items, vpt->size, vpt->dist_fn);
}

vpt_t VPT_nn(VPTree* vpt, vpt_t datapoint);

/* Returns a buffer (that must be freed) of MIN(k, vpt->size) items. */
vpt_t* VPT_knn(VPTree* vpt, vpt_t datapoint, size_t k);

void VPT_add(VPTree* vpt, vpt_t datapoint) {
    double dist;
    VPNode* current_node = vpt->root;

    vpt->size++;
    while (true) {
        if (current_node->ulabel == 'l') {
            if (current_node->u.pointlist.size >= current_node->u.pointlist.capacity) {
            } else {
                current_node->u.pointlist.items[current_node->u.pointlist.size++] = datapoint;
            }

        } else {
            dist = vpt->dist_fn(datapoint, current_node->u.branch.item);
            if (dist <= current_node->u.branch.radius) {
                current_node = current_node->u.branch.left;
            } else {
                current_node = current_node->u.branch.right;
            }
        }
    }
}

#endif
