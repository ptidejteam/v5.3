/*
 * ============================================================================
 * GNU Lesser General Public License
 * ============================================================================
 *
 * JasperReports - Free Java report-generating library.
 * Copyright (C) 2001-2006 JasperSoft Corporation http://www.jaspersoft.com
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 * 
 * JasperSoft Corporation
 * 303 Second Street, Suite 450 North
 * San Francisco, CA 94107
 * http://www.jaspersoft.com
 */
import java.awt.BorderLayout;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.URL;

import javax.swing.JOptionPane;

import net.sf.jasperreports.engine.JasperPrint;
import net.sf.jasperreports.engine.util.JRLoader;


/**
 * @author Teodor Danciu (teodord@users.sourceforge.net)
 * @version $Id: EmbeddedViewerApplet.java,v 1.1 2008/09/29 16:21:54 guehene Exp $
 */
public class EmbeddedViewerApplet extends javax.swing.JApplet
{


	/**
	 *
	 */
	private JasperPrint jasperPrint = null;


	/** Creates new form AppletViewer */
	public EmbeddedViewerApplet()
	{
		initComponents();
	}


	/**
	*
	*/
	public void init()
	{
		String url = getParameter("REPORT_URL");
		if (url != null)
		{
			try
			{
				jasperPrint = (JasperPrint)JRLoader.loadObject(new URL(getCodeBase(), url));
				if (jasperPrint != null)
				{
					JRViewerSimple viewer = new JRViewerSimple(jasperPrint);
					this.pnlMain.add(viewer, BorderLayout.CENTER);
				}
			}
			catch (Exception e)
			{
				StringWriter swriter = new StringWriter();
				PrintWriter pwriter = new PrintWriter(swriter);
				e.printStackTrace(pwriter);
				JOptionPane.showMessageDialog(this, swriter.toString());
			}
		}
		else
		{
			JOptionPane.showMessageDialog(this, "Source URL not specified");
		}
	}


	/** This method is called from within the constructor to
	 * initialize the form.
	 * WARNING: Do NOT modify this code. The content of this method is
	 * always regenerated by the Form Editor.
	 */
	private void initComponents() {//GEN-BEGIN:initComponents
		pnlMain = new javax.swing.JPanel();

		pnlMain.setLayout(new java.awt.BorderLayout());

		getContentPane().add(pnlMain, java.awt.BorderLayout.CENTER);

	}//GEN-END:initComponents
	
	
	// Variables declaration - do not modify//GEN-BEGIN:variables
	private javax.swing.JPanel pnlMain;
	// End of variables declaration//GEN-END:variables
	
}
