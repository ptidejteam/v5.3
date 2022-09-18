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
package net.sf.jasperreports.engine.base;

import net.sf.jasperreports.engine.JRConstants;
import net.sf.jasperreports.engine.JRExpression;
import net.sf.jasperreports.engine.JRExpressionCollector;
import net.sf.jasperreports.engine.JRSubreport;
import net.sf.jasperreports.engine.JRSubreportParameter;
import net.sf.jasperreports.engine.JRSubreportReturnValue;
import net.sf.jasperreports.engine.JRVisitor;
import net.sf.jasperreports.engine.util.JRStyleResolver;


/**
 * @author Teodor Danciu (teodord@users.sourceforge.net)
 * @version $Id: JRBaseSubreport.java,v 1.1 2008/09/29 16:21:17 guehene Exp $
 */
public class JRBaseSubreport extends JRBaseElement implements JRSubreport
{


	/**
	 *
	 */
	private static final long serialVersionUID = JRConstants.SERIAL_VERSION_UID;
	
	public static final String PROPERTY_USING_CACHE = "usingCache";

	/**
	 *
	 */
	protected Boolean isUsingCache = null;

	/**
	 *
	 */
	protected JRExpression parametersMapExpression = null;
	protected JRSubreportParameter[] parameters = null;
	protected JRExpression connectionExpression = null;
	protected JRExpression dataSourceExpression = null;
	protected JRExpression expression = null;
	
	/**
	 * Values to be copied from the subreport into the master report.
	 */
	protected JRSubreportReturnValue[] returnValues = null;


	/**
	 *
	 */
	public byte getMode()
	{
		return JRStyleResolver.getMode(this, MODE_TRANSPARENT);
	}


	/**
	 *
	 */
	protected JRBaseSubreport(JRSubreport subreport, JRBaseObjectFactory factory)
	{
		super(subreport, factory);
		
		this.isUsingCache = subreport.isOwnUsingCache();

		this.parametersMapExpression = factory.getExpression(subreport.getParametersMapExpression());

		/*   */
		JRSubreportParameter[] jrSubreportParameters = subreport.getParameters();
		if (jrSubreportParameters != null && jrSubreportParameters.length > 0)
		{
			this.parameters = new JRSubreportParameter[jrSubreportParameters.length];
			for(int i = 0; i < this.parameters.length; i++)
			{
				this.parameters[i] = factory.getSubreportParameter(jrSubreportParameters[i]);
			}
		}

		this.connectionExpression = factory.getExpression(subreport.getConnectionExpression());
		this.dataSourceExpression = factory.getExpression(subreport.getDataSourceExpression());
		
		JRSubreportReturnValue[] subrepReturnValues = subreport.getReturnValues();
		if (subrepReturnValues != null && subrepReturnValues.length > 0)
		{
			this.returnValues = new JRSubreportReturnValue[subrepReturnValues.length];
			for (int i = 0; i < subrepReturnValues.length; i++)
			{
				this.returnValues[i] = factory.getSubreportReturnValue(subrepReturnValues[i]);
			}
		}
		
		this.expression = factory.getExpression(subreport.getExpression());
	}
		

	/**
	 *
	 */
	public boolean isUsingCache()
	{
		if (this.isUsingCache == null)
		{
			JRExpression subreportExpression = getExpression();
			if (subreportExpression != null)
			{
				return String.class.getName().equals(subreportExpression.getValueClassName());
			}
			return true;
		}
		return this.isUsingCache.booleanValue();
	}


	/**
	 * @deprecated Replaced by {@link #setUsingCache(Boolean)}.
	 */
	public void setUsingCache(boolean isUsingCache)
	{
		setUsingCache(isUsingCache ? Boolean.TRUE : Boolean.FALSE);
	}


	/**
	 *
	 */
	public JRExpression getParametersMapExpression()
	{
		return this.parametersMapExpression;
	}


	/**
	 *
	 */
	public JRSubreportParameter[] getParameters()
	{
		return this.parameters;
	}


	/**
	 *
	 */
	public JRExpression getConnectionExpression()
	{
		return this.connectionExpression;
	}


	/**
	 *
	 */
	public JRExpression getDataSourceExpression()
	{
		return this.dataSourceExpression;
	}


	/**
	 *
	 */
	public JRExpression getExpression()
	{
		return this.expression;
	}
	
	/**
	 *
	 */
	public void collectExpressions(JRExpressionCollector collector)
	{
		collector.collect(this);
	}

	/**
	 *
	 */
	public void visit(JRVisitor visitor)
	{
		visitor.visitSubreport(this);
	}

	
	/**
	 * Returns the list of values to be copied from the subreport into the master.
	 * 
	 * @return the list of values to be copied from the subreport into the master.
	 */
	public JRSubreportReturnValue[] getReturnValues()
	{
		return this.returnValues;
	}


	public Boolean isOwnUsingCache()
	{
		return this.isUsingCache;
	}


	public void setUsingCache(Boolean isUsingCache)
	{
		Object old = this.isUsingCache;
		this.isUsingCache = isUsingCache;
		getEventSupport().firePropertyChange(PROPERTY_USING_CACHE, old, this.isUsingCache);
	}


	/**
	 * 
	 */
	public Object clone() 
	{
		JRBaseSubreport clone = (JRBaseSubreport)super.clone();
		
		if (this.parameters != null)
		{
			clone.parameters = new JRSubreportParameter[this.parameters.length];
			for(int i = 0; i < this.parameters.length; i++)
			{
				clone.parameters[i] = (JRSubreportParameter)this.parameters[i].clone();
			}
		}

		if (this.returnValues != null)
		{
			clone.returnValues = new JRSubreportReturnValue[this.returnValues.length];
			for(int i = 0; i < this.returnValues.length; i++)
			{
				clone.returnValues[i] = (JRSubreportReturnValue)this.returnValues[i].clone();
			}
		}

		if (this.parametersMapExpression != null)
		{
			clone.parametersMapExpression = (JRExpression)this.parametersMapExpression.clone();
		}
		if (this.connectionExpression != null)
		{
			clone.connectionExpression = (JRExpression)this.connectionExpression.clone();
		}
		if (this.dataSourceExpression != null)
		{
			clone.dataSourceExpression = (JRExpression)this.dataSourceExpression.clone();
		}
		if (this.expression != null)
		{
			clone.expression = (JRExpression)this.expression.clone();
		}

		return clone;
	}
}