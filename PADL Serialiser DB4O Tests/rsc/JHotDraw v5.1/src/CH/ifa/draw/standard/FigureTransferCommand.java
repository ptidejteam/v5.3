/*
 * @(#)FigureTransferCommand.java 5.1
 *
 */

package CH.ifa.draw.standard;

import java.util.Vector;

import CH.ifa.draw.framework.DrawingView;
import CH.ifa.draw.framework.Figure;
import CH.ifa.draw.framework.FigureEnumeration;
import CH.ifa.draw.framework.FigureSelection;
import CH.ifa.draw.util.Clipboard;
import CH.ifa.draw.util.Command;

/**
 * Common base clase for commands that transfer figures
 * between a drawing and the clipboard.
 */
abstract class FigureTransferCommand extends Command {

	protected DrawingView fView;

   /**
	* Constructs a drawing command.
	* @param name the command name
	* @param view the target view
	*/
	protected FigureTransferCommand(String name, DrawingView view) {
		super(name);
		fView = view;
	}

   /**
	* Deletes the selection from the drawing.
	*/
	protected void deleteSelection() {
	   fView.drawing().removeAll(fView.selection());
	   fView.clearSelection();
	}

   /**
	* Copies the selection to the clipboard.
	*/
	protected void copySelection() {
		FigureSelection selection = fView.getFigureSelection();
		Clipboard.getClipboard().setContents(selection);
	}

   /**
	* Inserts a vector of figures and translates them by the
	* given offset.
	*/
	protected void insertFigures(Vector figures, int dx, int dy) {
		FigureEnumeration e = new FigureEnumerator(figures);
		while (e.hasMoreElements()) {
			Figure figure = e.nextFigure();
			figure.moveBy(dx, dy);
			figure = fView.add(figure);
			fView.addToSelection(figure);
		}
	}

}


