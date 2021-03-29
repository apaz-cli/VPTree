#ifndef __VPTree
#define __VPTree

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

#ifndef vpt_t
#define vpt_t void*
#endif

#define VPT_BUILD_LIST_THRESHOLD 100
#define VPT_MAX_HEIGHT 100
#define VPT_MAX_LIST_SIZE 1000

#define NODEALLOC_BUF_SIZE 1000
#define LISTALLOC_BUF_SIZE 1000000

/**********************/
/* Struct Definitions */
/**********************/

struct VPNode;
typedef struct VPNode VPNode;
union VPNodeUnion;
typedef union VPNodeUnion VPNodeUnion;
struct VPBranch;
typedef struct VPBranch VPBranch;
struct PList;
typedef struct PList PList;

// This is a labeled union, containing either a branch in the tree, or a point list.
struct VPNode {  // 40 with vpt_t = void*
    char ulabel;
    union VPNodeUnion {
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

// Linked list for node allocations, built from the front
struct NodeAllocs;
typedef struct NodeAllocs NodeAllocs;
struct NodeAllocs {
    VPNode buffer[NODEALLOC_BUF_SIZE];
    size_t size;
    NodeAllocs* next;
};

struct ListAllocs;
typedef struct ListAllocs ListAllocs;
struct ListAllocs {
    vpt_t buffer[LISTALLOC_BUF_SIZE];
    size_t size;
    ListAllocs* next;
};

struct VPAllocator {
    NodeAllocs* node_allocs;
    ListAllocs* list_allocs;
};
typedef struct VPAllocator VPAllocator;

struct VPTree {
    VPNode* root;
    size_t size;
    VPAllocator allocator;
    void* extra_data;
    double (*dist_fn)(void* extra_data, vpt_t first, vpt_t second);
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

static inline size_t
min(size_t x, size_t y) {
    return x < y ? x : y;
}

static inline size_t
max(size_t x, size_t y) {
    return x > y ? x : y;
}

#include "vpsort.h"

/******************************/
/* Optimized Memory Allocator */
/******************************/

static inline VPNode*
__alloc_VPNode(VPAllocator* allocator) {
    // Get the current allocator struct.
    // If the one currently stored is full, build
    // another off of it and use that instead.
    NodeAllocs* alloc_list = allocator->node_allocs;
    if (alloc_list->size == NODEALLOC_BUF_SIZE - 1) {
        // Create an empty list and point it at the vptree's.
        NodeAllocs* new_list = malloc(sizeof(NodeAllocs));
        if (!new_list) return NULL;
        new_list->size = 0;
        new_list->next = allocator->node_allocs;

        // Replace the vptree allocation list
        allocator->node_allocs = new_list;
        alloc_list = new_list;
    }

    // Return a pointer to the next area for a node.
    VPNode* nodeptr = alloc_list->buffer + alloc_list->size;
    alloc_list->size++;

    debug_printf("Allocated node.\n");
    return nodeptr;
}

static inline vpt_t*
__alloc_VPList(VPAllocator* allocator, size_t buf_size) {
    // There's currently no way for this to happen.
    // if (buf_size > LISTALLOC_BUF_SIZE-1) return NULL;

    // Get the current VPList allocator
    ListAllocs* list_allocs = allocator->list_allocs;
    if (list_allocs->size + buf_size >= LISTALLOC_BUF_SIZE - 1) {
        // Create a new one if the old one would be overflown
        ListAllocs* new_list = malloc(sizeof(ListAllocs));
        if (!new_list) return NULL;
        new_list->size = 0;
        new_list->next = allocator->list_allocs;

        // Replace the one in the allocator with the new one and use it instead.
        allocator->list_allocs = new_list;
        list_allocs = new_list;
    }

    // Return a pointer to the start of the available memory space.
    vpt_t* allocated_list = (*list_allocs).buffer + list_allocs->size;
    list_allocs->size += buf_size;

    debug_printf("Allocated a VPList buffer of size %zu.\n", buf_size);
    return allocated_list;
}

/**********************/
/* Internal Functions */
/**********************/

static inline bool
__VPT_small_build(VPTree* vpt, vpt_t* data, size_t num_items) {
    vpt->root = __alloc_VPNode(&(vpt->allocator));
    if (!vpt->root) return NULL;
    vpt->root->ulabel = 'l';
    vpt->root->u.pointlist.size = num_items;
    vpt->root->u.pointlist.capacity = VPT_MAX_LIST_SIZE;
    vpt->root->u.pointlist.items = __alloc_VPList(&(vpt->allocator), vpt->root->u.pointlist.capacity);
    if (!vpt->root->u.pointlist.items) return NULL;
    for (size_t i = 0; i < num_items; i++) {
        vpt->root->u.pointlist.items[i] = data[i];
    }
    return true;
}

// Sorts a single element into position from just outside the list.
// Not suitable for knn. Operates on a list that has already been constructed sorted.
static inline void
__knnlist_push(VPEntry* knnlist, size_t knnlist_size, VPEntry to_add) {
    // Put the item right outside the list. We have allocated beyond the list, so this is okay.
    knnlist[knnlist_size] = to_add;

    if (!knnlist_size) return;
    size_t n = knnlist_size;

    // Shift the item inwards
    VPEntry temp;
    do {
        if (knnlist[n].distance < knnlist[n - 1].distance) {
            temp = knnlist[n];
            knnlist[n] = knnlist[n - 1];
            knnlist[n - 1] = temp;
        } else
            return;

        n--;
    } while (n);

    assert_sorted(knnlist, knnlist_size);
}

/****************/
/* Tree Methods */
/****************/

/**
 * Constructs a Vantage Point Tree out of the given data.
 * Destroy this tree using VPT_destroy or VPT_teardown.
 * 
 * This tree can store data of any type vpt_t for which the 
 * set of possible items forms a metric space with distance 
 * function dist_fn. You should #define vpt_t to be the type 
 * that the tree should store before you #include "vpt.h".
 * If you don't, the default is void*.
 * 
 * @param vpt The Vantage Point Tree to build.
 * @param data A pointer to the data to construct the tree out of. 
 * @param num_items The size of the data array.                
 * @param dist_fn A metric on the metric space of values of vpt_t.
 * @param extra_data Additional information to be passed to the dist_fn callback.
 * @return true if building the tree was successful, false if out of memory.
 *             No guaruntees on the state of the tree on failure.
 */
static inline bool
VPT_build(VPTree* vpt, vpt_t* data, size_t num_items,
          double (*dist_fn)(void* extra_data, vpt_t first, vpt_t second), void* extra_data) {
    if (!num_items) return true;
    vpt->size = num_items;
    vpt->dist_fn = dist_fn;
    vpt->extra_data = extra_data;

    /* Init allocator */
    vpt->allocator.node_allocs = malloc(sizeof(NodeAllocs));
    if (!vpt->allocator.node_allocs) return NULL;
    vpt->allocator.node_allocs->size = 0;
    vpt->allocator.node_allocs->next = NULL;

    vpt->allocator.list_allocs = malloc(sizeof(ListAllocs));
    if (!vpt->allocator.list_allocs) return NULL;
    vpt->allocator.list_allocs->size = 0;
    vpt->allocator.list_allocs->next = NULL;

    if (num_items < VPT_MAX_LIST_SIZE) {
        LOG("Building small tree of size %lu.\n", num_items)
        return __VPT_small_build(vpt, data, num_items);
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
    VPEntry* entry_list = build_buffer = malloc(num_entries * sizeof(VPEntry));
    if (!entry_list) return NULL;
    for (i = 0; i < num_entries; i++) {
        entry_list[i].item = data[i];
    }
    LOGs("Entry list copied.");

    // Allocate some space to help with sorting.
    VPEntry* scratch_space = malloc(num_entries * sizeof(VPEntry));
    if (!scratch_space) return NULL;
    LOGs("Allocated scratch space.");

    // Split the list in half using the root as a vantage point.
    // Sort the list by distance to the root
    for (i = 0; i < num_entries; i++)
        entry_list[i].distance = dist_fn(extra_data, entry_list[i].item, sort_by);
    VPSort(entry_list, num_entries, scratch_space);
    LOGs("Entry list sorted.");

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
    newnode = __alloc_VPNode(&(vpt->allocator));
    if (!newnode) return NULL;
    newnode->ulabel = 'b';
    newnode->u.branch.item = sort_by;
    newnode->u.branch.radius = (right_children - 1)->distance;
    vpt->root = newnode;

    // Push onto the stack the work that needs to be done to create the left and right of the root.
    leftstack[0].children = left_children;
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
                newnode = __alloc_VPNode(&(vpt->allocator));
                if (!newnode) return NULL;
                newnode->ulabel = 'l';
                newnode->u.pointlist.size = newnode->u.pointlist.capacity = popped.num_children;
                newnode->u.pointlist.items = __alloc_VPList(&(vpt->allocator), popped.num_children);
                if (!newnode->u.pointlist.items) return NULL;
                for (i = 0; i < popped.num_children; i++) {
                    newnode->u.pointlist.items[i] = popped.children[i].item;
                }
                popped.parent->u.branch.left = newnode;
                LOGs("Created leaf.");
            }

            // Inductive case, build node and push more information
            else {
                newnode = __alloc_VPNode(&(vpt->allocator));
                if (!newnode) return NULL;

                // Pop the first child off the list and into the new node
                sort_by = popped.children[0].item;
                entry_list = (popped.children + 1);
                num_entries = (popped.num_children - 1);

                // Sort the entries by the popped node
                for (i = 0; i < num_entries; i++)
                    entry_list[i].distance =
                        dist_fn(extra_data, sort_by, entry_list[i].item);
                VPSort(entry_list, num_entries, scratch_space);

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
                leftstack[left_stacksize].children = left_children;
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
                newnode = __alloc_VPNode(&(vpt->allocator));
                if (!newnode) return NULL;

                newnode->ulabel = 'l';
                newnode->u.pointlist.size = popped.num_children;
                newnode->u.pointlist.capacity = popped.num_children;
                newnode->u.pointlist.items = __alloc_VPList(&(vpt->allocator), popped.num_children);
                for (i = 0; i < popped.num_children; i++) {
                    newnode->u.pointlist.items[i] = popped.children[i].item;
                }
                popped.parent->u.branch.right = newnode;
                LOGs("Created leaf.");
            }

            // Inductive case, build node and push more information
            else {
                newnode = __alloc_VPNode(&(vpt->allocator));
                if (!newnode) return NULL;

                // Pop the first child off the list and into the new node
                sort_by = popped.children[0].item;
                entry_list = (popped.children + 1);
                num_entries = (popped.num_children - 1);

                // Sort the entries by the popped node
                for (i = 0; i < num_entries; i++)
                    entry_list[i].distance =
                        dist_fn(extra_data, sort_by, entry_list[i].item);
                VPSort(entry_list, num_entries, scratch_space);

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
                leftstack[left_stacksize].children = left_children;
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

    free(scratch_space);
    free(build_buffer);
    return true;
}

/**
 * Frees the resources owned by this VPTree. 
 *
 * The VPTree is not usable after this function is called. However, you can reuse 
 * the struct to build a new one via VPT_build. 
 * 
 * @param vpt The Vantage Point Tree to destroy.
 */
static inline void
VPT_destroy(VPTree* vpt) {
    NodeAllocs* node_allocs = vpt->allocator.node_allocs;
    while (node_allocs) {
        NodeAllocs* consumed_node_allocs = node_allocs;
        node_allocs = node_allocs->next;
        free(consumed_node_allocs);
    }

    ListAllocs* list_allocs = vpt->allocator.list_allocs;
    while (list_allocs) {
        ListAllocs* consumed_list_allocs = list_allocs;
        list_allocs = list_allocs->next;
        free(consumed_list_allocs);
    }

    LOGs("Tree destruction complete.");
}

/**
 * Frees the resources owned by this VPTree, and returns 
 * a buffer containing all the items that were inside. 
 * The size of the buffer returned is the same as vpt->size.
 * 
 * The VPTree is not usable after this function is called. However, you can reuse 
 * the struct to build a new one via VPT_build. Until you do so, you can also access
 * the previous size of the tree through vpt->size.
 * 
 * The buffer of items returned by this function is allocated with malloc() 
 * and must be freed. 
 * 
 * @param vpt The Vantage Point Tree to destroy.
 * @return The items that were contained in the tree, or NULL if out of memory.
 *             If this method returns null (there's not enough memory to 
 *             allocate the buffer to return), the tree will not be destroyed 
 *             and will remain usable.
 */
static inline vpt_t*
VPT_teardown(VPTree* vpt) {
    size_t all_size = 0;
    vpt_t* all_items = malloc(sizeof(vpt_t) * vpt->size);
    if (!all_items) {
        debug_printf("Failed to allocate memory to store data from the tree. Cannot return items, tearing down instead.\n");
        VPT_destroy(vpt);
        return NULL;
    }

    NodeAllocs* node_allocs = vpt->allocator.node_allocs;
    while (node_allocs) {
        for (size_t i = 0; i < node_allocs->size; i++) {
            VPNode node = node_allocs->buffer[i];
            if (node.ulabel == 'b') {
                vpt_t item = node.u.branch.item;
                all_items[all_size++] = item;
            } else {
                size_t listsize = node.u.pointlist.size;
                for (size_t j = 0; j < listsize; j++) {
                    vpt_t item = node.u.pointlist.items[j];
                    all_items[all_size++] = item;
                }
            }
        }
        NodeAllocs* consumed_alloc_list = node_allocs;
        node_allocs = node_allocs->next;
        free(consumed_alloc_list);
    }

    ListAllocs* list_allocs = vpt->allocator.list_allocs;
    while (list_allocs) {
        ListAllocs* consumed_list_allocs = list_allocs;
        list_allocs = list_allocs->next;
        free(consumed_list_allocs);
    }

    // Assert all_size == vpt->size
    LOGs("Tree disassembly complete.");
    return all_items;
}

/**
 * Rebuilds this Vantage Point Tree using the points already inside of it.
 * 
 * As you add points to the tree using VPT_add, the tree may become unbalanced.
 * At some point for efficiency of querying the tree, it becomes worth it to 
 * rebuild the tree to balance it.
 * 
 * @param vpt The Vantage Point Tree to rebuild.
 * @return true on success, false if out of memory.
 *             No guaruntees on the state of the tree on failure.
 */
bool VPT_rebuild(VPTree* vpt) {
    vpt_t* items = VPT_teardown(vpt);
    if (!items) return false;

    bool success = VPT_build(vpt, items, vpt->size, vpt->dist_fn, vpt->extra_data);
    if (!success) return false;

    free(items);
    return true;
}

/**
 * Performs a k-nearest-neighbor search on the Vantage Point Tree, finding the 
 * k closest datapoints in the tree to the datapoint provided, according to the 
 * VPTree's distance function.
 * 
 * You are expected to provide the buffer result_space, which the results are 
 * written to. It should be of size equal to or greater than the size of 
 * "VPEntry result_space[k];", which is to say (k * sizeof(VPEntry)) bytes.
 * 
 * The number of results found is written to num_results.
 */
static inline void
VPT_knn(VPTree* vpt, vpt_t datapoint, size_t k, VPEntry* result_space, size_t* num_results) {
    if (!vpt->size || !k) {
        *num_results = 0;
        return;
    }

    // Create a temp buffer
    size_t knnlist_size = 0;
    VPEntry knnlist[k + VPT_MAX_LIST_SIZE];

    // The largest distance to a knn
    double tau = INFINITY;

    // Scratch space for processing vplist distances
    double vplist_distances[VPT_MAX_LIST_SIZE];

    // Traverse the tree
    size_t to_traverse_size = 1;
    VPNode* to_traverse[VPT_MAX_HEIGHT];
    VPNode* current_node;
    to_traverse[0] = vpt->root;
    while (to_traverse_size) {
        // Pop a node from the stack
        current_node = to_traverse[--to_traverse_size];

        // Node is a branch
        if (current_node->ulabel == 'b') {
            // Calculate the distance between this branch node and the target point.
            double dist = vpt->dist_fn(vpt->extra_data, current_node->u.branch.item, datapoint);
            VPEntry current;
            current.item = current_node->u.branch.item;
            current.distance = dist;

            // Push the node we're visiting onto the list of candidates and
            // update tau when changes are made to the list.
            __knnlist_push(knnlist, knnlist_size, current);  // Minimal to no actual sorting
            knnlist_size = min(knnlist_size + 1, k);         // No branch on both x86 and ARM
            tau = knnlist[max(knnlist_size - 1, 0)].distance;

            // Debugging functions. These do nothing unless enabled and don't show up in the binary.
            assert_absolutely_sorted(knnlist, knnlist_size, datapoint, vpt);

            // Keep track of the parts of the tree that could still have nearest neighbors, and push
            // them onto the traversal stack. Keep doing this until we run out of tree to traverse.
            if (dist < current_node->u.branch.radius) {
                if (dist - tau <= current_node->u.branch.radius)
                    to_traverse[to_traverse_size++] = current_node->u.branch.left;
                if (dist + tau >= current_node->u.branch.radius)
                    to_traverse[to_traverse_size++] = current_node->u.branch.right;

            } else {
                if (dist + tau >= current_node->u.branch.radius)
                    to_traverse[to_traverse_size++] = current_node->u.branch.right;
                if (dist - tau <= current_node->u.branch.radius)
                    to_traverse[to_traverse_size++] = current_node->u.branch.left;
            }
        }

        // Node is a list
        else {
            size_t vplist_size = current_node->u.pointlist.size;
            vpt_t* vplist = current_node->u.pointlist.items;
            for (size_t i = 0; i < vplist_size; i++) {
                vplist_distances[i] = vpt->dist_fn(vpt->extra_data, vplist[i], datapoint);
            }

            // Update the new k nearest neighbors
            // 1. Break off the first k elements of the array (Already done)
            // 2. Sort the first k elements (they happen to have already been constructed sorted)
            assert_absolutely_sorted(knnlist, knnlist_size, datapoint, vpt);
            // 3. Keep track of the largest of the k items. This is tau, initialized earlier
            //    to infinity, or updated after the __knnlist_push() from the case for handling branch nodes.
            // 4. Iterate over the rest of the list. If the visited element is less than the
            // largest element of the first k, swap it in and shift it into place so the list
            // stays sorted. Then update the largest element.
            VPEntry entry;
            for (size_t i = 0; i < vplist_size; i++) {
                entry.item = vplist[i];
                entry.distance = vplist_distances[i];

                if (vplist_distances[i] < tau) {
                    __knnlist_push(knnlist, knnlist_size, entry);
                    knnlist_size = min(knnlist_size + 1, k);
                    tau = knnlist[max(knnlist_size - 1, 0)].distance;
                }
            }
        }
    }

    // Copy the results into the result space and return
    *num_results = knnlist_size;
    for (size_t i = 0; i < knnlist_size; i++)
        result_space[i] = knnlist[i];
}

/**
 * Performs a nearest-neighbor search on the Vantage Point Tree, finding the closest
 * datapoint in the tree to the one provided, according to the distance function for 
 * the VPTree.
 * 
 * The result is written to result_space, which should have enough space for a VPEntry.
 * 
 * If no result is found (the tree is empty), writes false to result_found. Otherwise, true.
 */
static inline void
VPT_nn(VPTree* vpt, vpt_t datapoint, VPEntry* result_space) {
    double dist;
    vpt_t closest;
    double closest_dist = INFINITY;

    size_t to_traverse_size = 1;
    VPNode* to_traverse[VPT_MAX_HEIGHT];
    VPNode* current_node;
    to_traverse[0] = vpt->root;

    // Traverse the tree
    while (to_traverse_size) {
        // Pop a node from the stack
        current_node = to_traverse[--to_traverse_size];

        // If branch
        if (current_node->ulabel == 'b') {
            // Calculate and consider this item's distance
            dist = vpt->dist_fn(vpt->extra_data, current_node->u.branch.item, datapoint);

            // Update new closest
            if (dist < closest_dist) {
                closest_dist = dist;
                closest = current_node->u.branch.item;
            }

            // Recurse down the tree
            if (dist < current_node->u.branch.radius) {
                if (dist - closest_dist <= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.left;
                }
                if (dist + closest_dist >= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.right;
                }
            } else {
                if (dist + closest_dist >= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.right;
                }
                if (dist - closest_dist <= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.left;
                }
            }
        }
        // If pointlist
        else {
            size_t listsize = current_node->u.pointlist.size;
            vpt_t* pointlist = current_node->u.pointlist.items;

            // Search for smaller items in the list
            for (size_t i = 0; i < listsize; i++) {
                dist = vpt->dist_fn(vpt->extra_data, pointlist[i], datapoint);

                if (dist < closest_dist) {
                    closest_dist = dist;
                    closest = pointlist[i];
                }
            }
        }
    }

    result_space->distance = closest_dist;
    result_space->item = closest;
}

/**
 * Adds a single element to an already constructed VPTree. 
 * 
 * The tree does not self-balance, so once you do this enough times you 
 * should call VPT_rebuild().
 *  
 * 
 */
static inline bool
VPT_add(VPTree* vpt, vpt_t to_add) {
    return false;  // Not implemented
}

/**
 * Rebuilds the given VPTree using the items that were already inside of 
 * it, plus the items to add. This has the effect of adding all the items.
 * 
 * For many items on a small tree, this will be faster than VPT_add on 
 * each item individually. But, it's best to benchmark.
 *
 * @param vpt The VPTree to modify.
 * @param to_add The array of items to add to the tree.
 * @param num_to_add The size of the array of items to add.
 * @return true on success, false if out of memory.
 */
static inline bool
VPT_add_rebuild(VPTree* vpt, vpt_t* to_add, size_t num_to_add) {
    return false;  // Not implemented
}
#endif
