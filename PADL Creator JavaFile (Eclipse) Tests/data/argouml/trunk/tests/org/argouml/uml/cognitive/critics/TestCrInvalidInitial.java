// $Id$
// Copyright (c) 2006 The Regents of the University of California. All
// Rights Reserved. Permission to use, copy, modify, and distribute this
// software and its documentation without fee, and without a written
// agreement is hereby granted, provided that the above copyright notice
// and this paragraph appear in all copies.  This software program and
// documentation are copyrighted by The Regents of the University of
// California. The software program and documentation are supplied "AS
// IS", without any accompanying services from The Regents. The Regents
// does not warrant that the operation of the program will be
// uninterrupted or error-free. The end-user understands that the program
// was developed for research purposes and is advised not to rely
// exclusively on the program for any reason.  IN NO EVENT SHALL THE
// UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
// ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
// THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE. THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE
// PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE UNIVERSITY OF
// CALIFORNIA HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT,
// UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

package org.argouml.uml.cognitive.critics;

import junit.framework.TestCase;

import org.argouml.model.Model;

public class TestCrInvalidInitial extends TestCase {

    private CrUML critic = null;

    private Object statemachine;

    private Object compositestate;

    private Object initial;

    private Object state1, state2;
    
    public TestCrInvalidInitial(String arg0) {
        super(arg0);
    }

    protected void setUp() throws Exception {
        super.setUp();
        critic = new CrInvalidInitial();
        statemachine = Model.getStateMachinesFactory().createStateMachine();
        compositestate = Model.getStateMachinesFactory()
                .buildCompositeStateOnStateMachine(statemachine);
        initial = Model.getStateMachinesFactory().buildPseudoState(
                compositestate);
        Model.getCoreHelper().setKind(initial,
                Model.getPseudostateKind().getInitial());
        state1 = Model.getStateMachinesFactory().buildSimpleState(
                compositestate);
        state2 = Model.getStateMachinesFactory().buildSimpleState(
                compositestate);
      

    }

    public void testPredicate2() {
        assertFalse(critic.predicate2(initial, null));
        Model.getStateMachinesFactory().buildTransition(initial, state1);
        assertFalse(critic.predicate2(initial, null));
        Model.getStateMachinesFactory().buildTransition(initial, state2);
        assertTrue(critic.predicate2(initial, null));
        
    }
}
