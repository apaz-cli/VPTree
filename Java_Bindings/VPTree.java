package vptree;

import java.util.Collection;
import java.util.List;
import java.util.function.BiFunction;
import java.io.Closeable;

public class VPTree<T> implements Closeable {
	static {
		System.loadLibrary("JVPTree");
	}

	/**
	 * Creates a Vantage Point Tree out of the items from the given collection.
	 * 
	 * The distance function may not return null. It also may not throw an
	 * exception. Violating these conditions is undefined behavior.
	 * 
	 * This data structure provides no guaruntee of thread safety. It must be
	 * externally synchronized.
	 * 
	 * For performance reasons, we do not check for exceptions after calling the
	 * distance function inside the native code. Therefore, it's recommended that
	 * the distance function never throw an exception under any circumstances. If it
	 * does, it's possible that the entire JVM could go haywire.
	 * 
	 * This includes unchecked exceptions, such as running out of memory or hitting
	 * recursion limits. It's recommended therefore not to create objects with "new"
	 * inside the distance function. This point is somewhat moot because if you run
	 * out of memory allocating an object, or hit recursion limit, the JVM will halt
	 * anyway. But if it happens, it may not crash in the way that you're used to.
	 * 
	 * @param coll
	 * @param distFn_nonnull_noexcept
	 */
	public VPTree(Collection<T> coll, BiFunction<T, T, Double> distFn_nonnull_noexcept) {
		VPT_build(coll.toArray(), distFn_nonnull_noexcept);
	}

	public native void rebuild();

	/**
	 * @return true if this tree is open, false if not.
	 */
	public boolean isOpen() {
		return vpt_ptr != 0;
	}

	/**
	 * @return The number of items in this tree.
	 */
	public native int size();

	///**
	// * Returns the items of the collection that were used to build the tree. This
	// * does not close the tree, and the tree does not give up its reference to the
	// * array of items.
	// * 
	// * @return items The items of the collection
	// */
	//public native T[] getItems();

	/**
	 * @param queryPoint The query point to find the nearest neighbor to
	 * @return The nearest neighbor, or null when the tree is empty or has already
	 *         been closed.
	 */
	public native VPEntry<T> nn(T queryPoint);

	/**
	 * Searches the tree for the k nearest neighbors to the given point, and returns
	 * a list of the items with their distances.
	 * 
	 * If the tree contains less than k items, then the size of the list will be
	 * equal to the size of the tree.
	 * 
	 * @param queryPoint The query point
	 * @param k          The number of nearest neighbors to return
	 * @return knnlist A list of the Math.min(this.size(), k) closest points in the
	 *         tree to the query point.
	 */
	public native List<VPEntry<T>> knn(T queryPoint, long k);

	/**
	 * Searches the tree for all points withing max_dist of the query point, and
	 * returns returns them and the distance to them.
	 * 
	 * @param queryPoint The point to get items close to.
	 * @param max_dist   The maximum distance from the query point to the items
	 *                   retrieved.
	 * @return withinlist A list of all the points p in the tree for which
	 *         dist_Fn(queryPoint, p) <= max_dist, along with the distances to those
	 *         points.
	 */
	public native List<VPEntry<T>> all_within(T queryPoint, double max_dist);

	/**
	 * Adds the given point to this Vantage Point Tree.
	 * 
	 * Note that no considerations are given for balancing the tree. If you add too
	 * many items this way, searching the tree will become inefficient. In that
	 * case, either call addRebuild() instead, or rebuild().
	 * 
	 * @param datapoint The point to add to the tree.
	 */
	public native void add(T datapoint);

	public void addRebuild(Collection<T> datapoints) {
		VPT_addRebuild(datapoints.toArray());
	}

	/**
	 * Frees the native memory allocated for this tree. You must close the tree once
	 * you are done with it.
	 */
	@Override
	public native void close();

	private native void VPT_build(Object[] objArr, BiFunction<T, T, Double> distFn);

	private native void VPT_addRebuild(Object[] datapoints);

	/********************************************/
	/* The following looks useless, but is not. */
	/* Native code depends on it. Do not touch. */
	/* If you do, things will break.            */
	/********************************************/

	// Native method VPT_build allocates memory for persistent storage and stores
	// the location here. Do not remove this, or things will break.
	private long vpt_ptr = 0L;

}
