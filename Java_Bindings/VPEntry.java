package vptree;

public class VPEntry<T> {
	public T item;
	public double distance;

	public VPEntry(T item, double distance) {
		this.item = item;
		this.distance = distance;
	};

	@Override
	public String toString() {
		return new StringBuilder()
				.append('<')
				.append(this.item.toString())
				.append(", ")
				.append(this.distance)
				.append('>')
				.toString();
	}

	@Override
	public boolean equals(Object o) {
		if (!(o instanceof VPEntry<?>)) return false;
		VPEntry<?> other = (VPEntry<?>) o;
		return this.item.equals(other.item) && (this.distance == other.distance);
	}

	@Override
	public int hashCode() {
		return this.item.hashCode() ^ (int) this.distance;
	}

	/********************************************/
	/* The following looks useless, but is not. */
	/* Native code depends on it. Do not touch. */
	/* If you do, things will break.            */
	/********************************************/

	@SuppressWarnings("unused")
	private static Object[] arrcat(Object[] arr1, Object[] arr2) {
		Object[] dest = new Object[arr1.length + arr2.length];
		System.arraycopy(arr1, 0, dest, 0, arr1.length);
		System.arraycopy(arr2, 0, dest, arr1.length, arr2.length);
		return dest;
	}

	@SuppressWarnings("unused")
	native Object[] arrpush(Object[] arr, Object pushing);
	/*
	{
		Object[] dest = new Object[arr.length+1];
		System.arraycopy(arr, 0, dest, 0, arr.length);
		dest[arr.length] = pushing;
		return dest;
	}*/
}
