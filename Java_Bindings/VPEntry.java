package vptree;

public class VPEntry<T> {
	public T item;
	public double distance;

	public VPEntry(T item, double distance) {
		this.item = item;
		this.distance = distance;
	};
}