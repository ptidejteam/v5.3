package ca.qc.frqnt.project.servicisation.model.test;

import ca.qc.frqnt.project.servicisation.model.Model;
import junit.framework.TestCase;

public class Test1 extends TestCase {
	@SuppressWarnings("unused")
	private static Model model;
	
	@Override
	protected void setUp() throws Exception {
		super.setUp();
		
		Test1.model = new Model();
		
	}
	
	
}
