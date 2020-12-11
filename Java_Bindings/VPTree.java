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

	public VPTree(Collection<T> coll, BiFunction<T, T, Double> distFn) {
		VPT_build(coll.toArray(), distFn);
	}

	private native void VPT_build(Object[] objArr, BiFunction<T, T, Double> distFn);

	public native VPEntry<T> nn(T datapoint);

	public native List<VPEntry<T>> knn(T datapoint);

	public native int size();
	
	@Override
	public native void close();
}
