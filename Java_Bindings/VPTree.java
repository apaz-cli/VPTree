package vptree;

import java.util.Collection;
import java.util.List;
import java.util.function.BiFunction;
import java.io.Closeable;

public class VPTree<T> implements Closeable {
	static {
		System.loadLibrary("VPTree");
	}

	// Native method VPT_build allocates memory for persistent storage and stores
	// the location here.
	private long vpt_ptr = 0L;

	// This distance function may never throw an exception under any circumstances.
	// If it does, it's possible that the entire JVM could go haywire.
	public VPTree(Collection<T> coll, BiFunction<T, T, Double> distFn_noexcept) {
		VPT_build(coll.toArray(), distFn_noexcept);
	}

	private native void VPT_build(Object[] objArr, BiFunction<T, T, Double> distFn);

	public native VPEntry<T> nn(T datapoint);

	public native List<VPEntry<T>> knn(T datapoint);

	public native int size();

	@Override
	public native void close();
}
