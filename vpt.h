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

#define VPT_BUILD_SMALL_THRESHOLD 250
#define VPT_BUILD_LIST_THRESHOLD 100
#define VPT_MAX_HEIGHT 100

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
        NodeAllocs* new_list = (NodeAllocs*)malloc(sizeof(NodeAllocs));
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

static inline bool
__VPT_small_build(VPTree* vpt, vpt_t* data, size_t num_items) {
    size_t i;
    vpt->root = alloc_VPNode(vpt);
    if (!vpt->root) return NULL;
    vpt->root->ulabel = 'l';
    vpt->root->u.pointlist.size = num_items;
    vpt->root->u.pointlist.capacity = sizeof(vpt_t) * VPT_BUILD_SMALL_THRESHOLD;
    vpt->root->u.pointlist.items = (vpt_t*)malloc(vpt->root->u.pointlist.capacity);
    if (!vpt->root->u.pointlist.items) return NULL;
    for (i = 0; i < num_items; i++) {
        vpt->root->u.pointlist.items[i] = data[i];
    }
    return true;
}

/****************/
/* Tree Methods */
/****************/

bool VPT_build(VPTree* vpt, vpt_t* data, size_t num_items,
               double (*dist_fn)(void* extra_data, vpt_t first, vpt_t second), void* extra_data) {
    if (num_items == 0) return NULL;
    vpt->size = num_items;
    vpt->dist_fn = dist_fn;
    vpt->extra_data = extra_data;
    vpt->node_allocs = (NodeAllocs*)malloc(sizeof(NodeAllocs));
    if (!vpt->node_allocs) return NULL;
    vpt->node_allocs->size = 0;
    vpt->node_allocs->next = NULL;

    if (num_items <= VPT_BUILD_SMALL_THRESHOLD) {
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
    VPEntry* entry_list = build_buffer = (VPEntry*)malloc(num_entries * sizeof(VPEntry));
    for (i = 0; i < num_entries; i++) {
        entry_list[i].item = data[i];
    }
    LOGs("Entry list copied.");

    // Split the list in half using the root as a vantage point.
    // Sort the list by distance to the root
    for (i = 0; i < num_entries; i++)
        entry_list[i].distance = dist_fn(extra_data, entry_list[i].item, sort_by);
    VPSort(entry_list, num_entries);
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
                newnode->u.pointlist.items = (vpt_t*)malloc(popped.num_children * sizeof(vpt_t));
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
                VPSort(entry_list, num_entries);

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
                newnode->u.pointlist.items = (vpt_t*)malloc(popped.num_children * sizeof(vpt_t));
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
                VPSort(entry_list, num_entries);

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
    free(build_buffer);
    return true;
}

void VPT_destroy(VPTree* vpt) {
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

vpt_t* VPT_teardown(VPTree* vpt) {
    size_t all_size = 0;
    vpt_t* all_items = (vpt_t*)malloc(sizeof(vpt_t) * vpt->size);
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

void VPT_rebuild(VPTree* vpt) {
    LOGs("Reclaiming items from tree.");
    vpt_t* items = VPT_teardown(vpt);
    LOGs("Rebuilding tree.");
    VPT_build(vpt, items, vpt->size, vpt->dist_fn, vpt->extra_data);
    free(items);
}

// Sorts a single element into position from just outside the list.
// Not suitable for knn. Operates on a list that has already been constructed sorted.
static inline void
knnlist_push(VPEntry* knnlist, size_t knnlist_size, VPEntry to_add) {
    // Put the item right outside the list. We have allocated beyond the list, so this is okay.
    knnlist[knnlist_size] = to_add;

    // Shift the item inwards
    VPEntry temp;
    size_t n = knnlist_size;
    do {
        if (knnlist[n].distance < knnlist[n - 1].distance) {
            temp = knnlist[n];
            knnlist[n] = knnlist[n - 1];
            knnlist[n - 1] = temp;
            print_list(knnlist, knnlist_size);
        } else {
            return;
        }
        n--;
    } while (n >= 1);

    assert_locally_sorted(knnlist, knnlist_size);
}

/* Returns a buffer (that must be freed) of MIN(k, vpt->size) items. Returns NULL if failed. */
VPEntry* VPT_knn(VPTree* vpt, vpt_t datapoint, size_t k) {
    if (vpt->size == 0 || k == 0) return NULL;

    size_t knnlist_size = 0;
    VPEntry* knnlist = (VPEntry*)malloc(sizeof(VPEntry) * (k + VPT_BUILD_LIST_THRESHOLD));
    if (!knnlist) return NULL;

    // The largest distance to a knn
    double tau = INFINITY;

    // Scratch space for processing vplist distances
    double vplist_distances[VPT_BUILD_LIST_THRESHOLD];

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
            double dist = vpt->dist_fn(vpt->extra_data, datapoint, current_node->u.branch.item);
            VPEntry current;
            current.item = current_node->u.branch.item;
            current.distance = dist;

            // Push the node we're visiting onto the list of candidates
            knnlist_push(knnlist, knnlist_size, current);  // Minimal to no actual sorting
            knnlist_size = min(knnlist_size + 1, k);       // No branch on both x86 and ARM

            // Update tau
            tau = knnlist[knnlist_size - 1].distance;

            // Debugging functions. These do nothing and don't show up in the binary.
            // print_list(knnlist, knnlist_size);
            assert_sorted(knnlist, knnlist_size, datapoint, vpt);

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
                vplist_distances[i] = vpt->dist_fn(vpt->extra_data, datapoint, vplist[i]);
            }

            // Update the new k nearest neighbors
            // 1. Break off the first k elements of the array (Already done)
            // 2. Sort the first k elements (they happen to have already been constructed sorted)
            assert_sorted(knnlist, knnlist_size, datapoint, vpt);
            // 3. Keep track of the largest of the k items (They're sorted, so we know where it is)
            double largest_k_dist = knnlist[knnlist_size - 1].distance;

            // 4. Iterate over the rest of the list. If the visited element is less than the
            // largest element of the first k, swap it in and shift it into place so the list
            // stays sorted. Then update the largest element.
            VPEntry entry;
            for (size_t i = 0; i < vplist_size; i++) {
                entry.item = vplist[i];
                entry.distance = vplist_distances[i];

                if (vplist_distances[i] < largest_k_dist) {
                    knnlist_push(knnlist, knnlist_size, entry);
                    knnlist_size = min(knnlist_size + 1, k);

                    largest_k_dist = knnlist[knnlist_size - 1].distance;
                }
            }

            // There's no need to update tau. We don't need it to do list knn, 
            // and it's always updated right before it's needed.
        }
    }

    return realloc(knnlist, sizeof(VPEntry) * min(k, knnlist_size));
}

VPEntry* VPT_nn(VPTree* vpt, vpt_t datapoint) {
    double dist;
    vpt_t* closest;
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
            dist = vpt->dist_fn(vpt->extra_data, datapoint, current_node->u.branch.item);

            // Update new closest
            if (dist < closest_dist) {
                debug_printf("Found new nearest neighbor: %f at %p.\n", closest_dist, closest);
                closest_dist = dist;
                closest = &(current_node->u.branch.item);
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
            size_t listsize = vpt->root->u.pointlist.size;
            vpt_t* pointlist = vpt->root->u.pointlist.items;

            // Search for smaller items in the list
            for (size_t i = 0; i < listsize; i++) {
                dist = vpt->dist_fn(vpt->extra_data, datapoint, pointlist[i]);

                if (dist < closest_dist) {
                    debug_printf("Found new nearest neighbor: %f at %p.\n", closest_dist, closest);
                    closest_dist = dist;
                    closest = pointlist + i;
                }
            }
        }
    }

    VPEntry* ret = (VPEntry*)malloc(sizeof(VPEntry));
    if (!ret) {
        LOGs("Failed to allocate memory for the result. Returning NULL instead.");
        return NULL;
    }
    ret->distance = closest_dist;
    ret->item = *(closest);
    return ret;
}
#endif
