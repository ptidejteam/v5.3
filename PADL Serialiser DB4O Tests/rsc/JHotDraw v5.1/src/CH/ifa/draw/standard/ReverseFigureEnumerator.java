/*
 * @(#)ReverseFigureEnumerator.java 5.1
 *
 */

package CH.ifa.draw.standard;

import java.util.Vector;

import CH.ifa.draw.framework.Figure;
import CH.ifa.draw.framework.FigureEnumeration;
import CH.ifa.draw.util.ReverseVectorEnumerator;

/**
 * An Enumeration that enumerates a vector of figures back (size-1) to front (0).
 */
public final class ReverseFigureEnumerator implements FigureEnumeration {
	ReverseVectorEnumerator fEnumeration;

	public ReverseFigureEnumerator(Vector v) {
	    fEnumeration = new ReverseVectorEnumerator(v);
	}

	/**
	 * Returns true if the enumeration contains more elements; false
	 * if its empty.
	 */
	public boolean hasMoreElements() {
	    return fEnumeration.hasMoreElements();
	}

	/**
	 * Returns the next element of the enumeration. Calls to this
	 * method will enumerate successive elements.
	 * @exception NoSuchElementException If no more elements exist.
	 */
	public Object nextElement() {
		return fEnumeration.nextElement();
	}

	/**
	 * Returns the next element casted as a figure of the enumeration. Calls to this
	 * method will enumerate successive elements.
	 * @exception NoSuchElementException If no more elements exist.
	 */
	public Figure nextFigure() {
		return (Figure)fEnumeration.nextElement();
	}
}
