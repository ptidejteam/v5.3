/*
 * @(#)TextHolder.java 5.1
 *
 */

package CH.ifa.draw.standard;

import java.awt.Font;
import java.awt.Rectangle;

import CH.ifa.draw.framework.Figure;

/**
 * The interface of a figure that has some editable text contents.
 * @see Figure
 */

public interface TextHolder {

	public Rectangle textDisplayBox();

	/**
	 * Gets the text shown by the text figure.
	 */
	public String getText();

	/**
	 * Sets the text shown by the text figure.
	 */
	public void setText(String newText);

	/**
	 * Tests whether the figure accepts typing.
	 */
	public boolean acceptsTyping();

	/**
	 * Gets the number of columns to be overlaid when the figure is edited.
	 */
	public int overlayColumns();

	/**
	 * Connects a figure to another figure.
	 */
	public void connect(Figure connectedFigure);

	/**
	 * Gets the font.
	 */
	public Font getFont();

}
