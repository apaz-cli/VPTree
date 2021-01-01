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
// #define LISTALLOC_BUF_SIZE 100000

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

// Linked list for node allocations, built from the front
struct NodeAllocs;
typedef struct NodeAllocs NodeAllocs;
struct NodeAllocs {
    VPNode buffer[NODEALLOC_BUF_SIZE];
    size_t size;
    NodeAllocs* next;
};

struct VPTree {
    VPNode* root;
    size_t size;
    NodeAllocs* node_allocs;
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

#include "vpsort.h"

/*******************************/
/* Optimized Memory Allocators */
/*******************************/

static inline VPNode*
alloc_VPNode(VPTree* vpt) {
    // Get the current allocation list. If the one currently stored is full, build another off of it.
    NodeAllocs* alloc_list = vpt->node_allocs;
    if (alloc_list->size == NODEALLOC_BUF_SIZE - 1) {
        // Create an empty list and point it at the vptree's.
        NodeAllocs* new_list = malloc(sizeof(NodeAllocs));
        if (!new_list) return NULL;
        new_list->size = 0;
        new_list->next = vpt->node_allocs;

        // Replace the vptree allocation list
        vpt->node_allocs = new_list;
        alloc_list = new_list;
    }

    // Return a pointer to the next area for a node.
    VPNode* nodeptr = alloc_list->buffer + alloc_list->size;
    alloc_list->size++;

    LOGs("Allocated node.");
    return nodeptr;
}

static inline size_t
min(size_t x, size_t y) {
    return x < y ? x : y;
}

static inline size_t
max(size_t x, size_t y) {
    return x > y ? x : y;
}

static inline bool
__VPT_small_build(VPTree* vpt, vpt_t* data, size_t num_items) {
    vpt->root = alloc_VPNode(vpt);
    if (!vpt->root) return NULL;
    vpt->root->ulabel = 'l';
    vpt->root->u.pointlist.size = num_items;
    vpt->root->u.pointlist.capacity = VPT_MAX_LIST_SIZE;
    vpt->root->u.pointlist.items = malloc(vpt->root->u.pointlist.capacity * sizeof(vpt_t));
    if (!vpt->root->u.pointlist.items) return NULL;
    for (size_t i = 0; i < num_items; i++) {
        vpt->root->u.pointlist.items[i] = data[i];
    }
    return true;
}

/****************/
/* Tree Methods */
/****************/

static inline bool
VPT_build(VPTree* vpt, vpt_t* data, size_t num_items,
          double (*dist_fn)(void* extra_data, vpt_t first, vpt_t second), void* extra_data) {
    if (num_items == 0) return NULL;
    vpt->size = num_items;
    vpt->dist_fn = dist_fn;
    vpt->extra_data = extra_data;
    vpt->node_allocs = malloc(sizeof(NodeAllocs));
    if (!vpt->node_allocs) return NULL;
    vpt->node_allocs->size = 0;
    vpt->node_allocs->next = NULL;

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
    newnode = alloc_VPNode(vpt);
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
                // Find a smarter way to allocate memory than by peppering the heap
                // with like 5 million individually allocated nodes
                newnode = alloc_VPNode(vpt);
                if (!newnode) return NULL;
                newnode->ulabel = 'l';
                newnode->u.pointlist.size = newnode->u.pointlist.capacity = popped.num_children;
                newnode->u.pointlist.items = malloc(popped.num_children * sizeof(vpt_t));
                if (!newnode->u.pointlist.items) return NULL;
                for (i = 0; i < popped.num_children; i++) {
                    newnode->u.pointlist.items[i] = popped.children[i].item;
                }
                popped.parent->u.branch.left = newnode;
                LOGs("Created leaf.");
            }

            // Inductive case, build node and push more information
            else {
                newnode = alloc_VPNode(vpt);
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
                newnode = alloc_VPNode(vpt);
                if (!newnode) return NULL;

                newnode->ulabel = 'l';
                newnode->u.pointlist.size = popped.num_children;
                newnode->u.pointlist.capacity = popped.num_children;
                newnode->u.pointlist.items = malloc(popped.num_children * sizeof(vpt_t));
                for (i = 0; i < popped.num_children; i++) {
                    newnode->u.pointlist.items[i] = popped.children[i].item;
                }
                popped.parent->u.branch.right = newnode;
                LOGs("Created leaf.");
            }

            // Inductive case, build node and push more information
            else {
                newnode = alloc_VPNode(vpt);
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
 * Frees the resources owned by this VPTree. The VPTree is not usable after this function is called.
 */
static inline void
VPT_destroy(VPTree* vpt) {
    NodeAllocs* alloc_list = vpt->node_allocs;
    while (alloc_list) {
        for (size_t i = 0; i < alloc_list->size; i++) {
            VPNode node = alloc_list->buffer[i];
            if (node.ulabel == 'l') {
                free(node.u.pointlist.items);
            }
        }
        NodeAllocs* consumed_alloc_list = alloc_list;
        alloc_list = alloc_list->next;
        free(consumed_alloc_list);
    }

    LOGs("Tree destruction complete.");
}

/**
 * Frees the resources owned by this VPTree, and returns a buffer containing all the items that were inside. 
 * The VPTree is not usable after this function is called. 
 * 
 * The buffer of items returned by this function must be freed. 
 * You can still access the number of items in the buffer using the size of the tree, although it was just destroyed.
 */
static inline vpt_t*
VPT_teardown(VPTree* vpt) {
    size_t all_size = 0;
    vpt_t* all_items = malloc(sizeof(vpt_t) * vpt->size);
    if (!all_items) {
        LOGs("Failed to allocate memory to store data from the tree. Cannot return items, tearing down instead.");
        VPT_destroy(vpt);
        return NULL;
    }

    NodeAllocs* alloc_list = vpt->node_allocs;
    while (alloc_list) {
        for (size_t i = 0; i < alloc_list->size; i++) {
            VPNode node = alloc_list->buffer[i];
            if (node.ulabel == 'b') {
                vpt_t item = node.u.branch.item;
                all_items[all_size++] = item;
            } else {
                size_t listsize = node.u.pointlist.size;
                for (size_t j = 0; j < listsize; j++) {
                    vpt_t item = node.u.pointlist.items[j];
                    all_items[all_size++] = item;
                }
                free(node.u.pointlist.items);
            }
        }
        NodeAllocs* consumed_alloc_list = alloc_list;
        alloc_list = alloc_list->next;
        free(consumed_alloc_list);
    }

    // Assert all_size == vpt->size
    LOGs("Tree disassembly complete.");
    return all_items;
}

bool VPT_rebuild(VPTree* vpt) {
    vpt_t* items = VPT_teardown(vpt);
    if (items == NULL) return false;

    bool success = VPT_build(vpt, items, vpt->size, vpt->dist_fn, vpt->extra_data);
    if (!success) return false;

    free(items);
    return true;
}

// Sorts a single element into position from just outside the list.
// Not suitable for knn. Operates on a list that has already been constructed sorted.
static inline void
knnlist_push(VPEntry* knnlist, size_t knnlist_size, VPEntry to_add) {
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

/**
 * Performs a k-nearest-neighbor search on the Vantage Point Tree, finding the k closest
 * datapoints in the tree to the datapoint provided, according to the distance function
 * for the VPTree.
 * 
 * The results are written to result_space, which should be a buffer of size equal to or greater
 * than "VPEntry result_space[k];", which is to say (k * sizeof(VPEntry*)) bytes large.
 * 
 * Also writes the number of results found to num_results.
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
            knnlist_push(knnlist, knnlist_size, current);  // Minimal to no actual sorting
            knnlist_size = min(knnlist_size + 1, k);       // No branch on both x86 and ARM
            tau = knnlist[max(knnlist_size - 1, 0)].distance;

            // Debugging functions. These do nothing unless enabled and don't show up in the binary.
            assert_absolutely_sorted(knnlist, knnlist_size, datapoint, vpt);

            // Keep track of the parts of the tree that could still have nearest neighbors, and push
            // them onto the traversal stack. Keep doing this until we run out of tree to traverse.
            if (dist < current_node->u.branch.radius) {
                if (dist - tau <= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.left;
                }
                if (dist + tau >= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.right;
                }
            } else {
                if (dist + tau >= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.right;
                }
                if (dist - tau <= current_node->u.branch.radius) {
                    to_traverse[to_traverse_size++] = current_node->u.branch.left;
                }
            }
        }

        // Node is a list
        else {
            vpt_t* vplist = current_node->u.pointlist.items;
            size_t vplist_size = current_node->u.pointlist.size;

            for (size_t i = 0; i < vplist_size; i++) {
                vplist_distances[i] = vpt->dist_fn(vpt->extra_data, vplist[i], datapoint);
            }

            // Update the new k nearest neighbors
            // 1. Break off the first k elements of the array (Already done)
            // 2. Sort the first k elements (they happen to have already been constructed sorted)
            assert_absolutely_sorted(knnlist, knnlist_size, datapoint, vpt);
            // 3. Keep track of the largest of the k items. This is tau, initialized earlier
            //    to infinity, or updated after the knnlist_push() from the case for handling branch nodes.
            // 4. Iterate over the rest of the list. If the visited element is less than the
            // largest element of the first k, swap it in and shift it into place so the list
            // stays sorted. Then update the largest element.
            VPEntry entry;
            for (size_t i = 0; i < vplist_size; i++) {
                entry.item = vplist[i];
                entry.distance = vplist_distances[i];

                if (vplist_distances[i] < tau) {
                    knnlist_push(knnlist, knnlist_size, entry);
                    knnlist_size = min(knnlist_size + 1, k);
                    tau = knnlist[max(knnlist_size - 1, 0)].distance;
                }
            }
        }
    }
    
    // Copy the results into the result space and return
    *num_results = knnlist_size;
    for (size_t i = 0; i < knnlist_size; i++) {
        result_space[i] = knnlist[i];
    }
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
#endif
