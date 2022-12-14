// $Id$
// Copyright (c) 1996-2006 The Regents of the University of California. All
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

package org.argouml.model.mdr;

import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Vector;

import javax.jmi.reflect.InvalidObjectException;

import org.argouml.model.CommonBehaviorHelper;
import org.argouml.model.InvalidElementException;
import org.argouml.model.Model;
import org.omg.uml.behavioralelements.collaborations.ClassifierRole;
import org.omg.uml.behavioralelements.collaborations.Message;
import org.omg.uml.behavioralelements.commonbehavior.Action;
import org.omg.uml.behavioralelements.commonbehavior.ActionSequence;
import org.omg.uml.behavioralelements.commonbehavior.Argument;
import org.omg.uml.behavioralelements.commonbehavior.AttributeLink;
import org.omg.uml.behavioralelements.commonbehavior.CallAction;
import org.omg.uml.behavioralelements.commonbehavior.ComponentInstance;
import org.omg.uml.behavioralelements.commonbehavior.CreateAction;
import org.omg.uml.behavioralelements.commonbehavior.Instance;
import org.omg.uml.behavioralelements.commonbehavior.Link;
import org.omg.uml.behavioralelements.commonbehavior.LinkEnd;
import org.omg.uml.behavioralelements.commonbehavior.NodeInstance;
import org.omg.uml.behavioralelements.commonbehavior.Reception;
import org.omg.uml.behavioralelements.commonbehavior.SendAction;
import org.omg.uml.behavioralelements.commonbehavior.Signal;
import org.omg.uml.behavioralelements.commonbehavior.Stimulus;
import org.omg.uml.behavioralelements.statemachines.CallEvent;
import org.omg.uml.behavioralelements.statemachines.Guard;
import org.omg.uml.behavioralelements.statemachines.SignalEvent;
import org.omg.uml.behavioralelements.statemachines.StateVertex;
import org.omg.uml.behavioralelements.statemachines.Transition;
import org.omg.uml.foundation.core.BehavioralFeature;
import org.omg.uml.foundation.core.Classifier;
import org.omg.uml.foundation.core.Operation;
import org.omg.uml.foundation.core.TaggedValue;
import org.omg.uml.foundation.datatypes.ActionExpression;
import org.omg.uml.foundation.datatypes.Expression;
import org.omg.uml.foundation.datatypes.IterationExpression;
import org.omg.uml.foundation.datatypes.ObjectSetExpression;

/**
 * The CommonBehaviorHelper for the MDR ModelImplementation.
 * <p>
 * @since ARGO0.19.5
 * @author Ludovic Ma&icirc;tre
 * @author Tom Morris
 * 
 */
public class CommonBehaviorHelperMDRImpl implements CommonBehaviorHelper {

    private MDRModelImplementation modelImpl;

    /**
     * Default constructor
     * 
     * @param implementation
     *            The model implementation
     */
    public CommonBehaviorHelperMDRImpl(MDRModelImplementation implementation) {
        this.modelImpl = implementation;
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#getSource(java.lang.Object)
     */
    public Object getSource(Object link) {
        try {
            if (link instanceof Link) {
                return modelImpl.getCoreHelper().getSource(link);
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Argument is not a link");
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#getDestination(java.lang.Object)
     */
    public Object getDestination(Object link) {
        try {
            if (link instanceof Link) {
                return modelImpl.getCoreHelper().getDestination(link);
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Argument is not a link");
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#removeActualArgument(java.lang.Object,
     *      java.lang.Object)
     */
    public void removeActualArgument(Object handle, Object argument) {
        try {
            if (handle instanceof Action && argument instanceof Argument) {
                ((Action) handle).getActualArgument().remove(argument);
                return;
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Unrecognized object " + handle
                + " or " + argument);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setActualArguments(java.lang.Object, java.util.List)
     */
    public void setActualArguments(Object action, List arguments) {
        try {
            if (action instanceof Action) {
                ((Action) action).getActualArgument().clear();
                ((Action) action).getActualArgument().addAll(arguments);
                return;
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Unrecognized object " + action
                + " or " + arguments);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#removeClassifier(java.lang.Object,
     *      java.lang.Object)
     */
    public void removeClassifier(Object handle, Object classifier) {
        try {
            if (handle instanceof Instance 
                    && classifier instanceof Classifier) {
                ((Instance) handle).getClassifier().remove(classifier);
                return;
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Unrecognized object " + handle
                + " or " + classifier);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#removeContext(java.lang.Object,
     *      java.lang.Object)
     */
    public void removeContext(Object handle, Object context) {
        try {
            if (handle instanceof Signal
                    && context instanceof BehavioralFeature) {
                modelImpl.getUmlPackage().getCommonBehavior()
                        .getAContextRaisedSignal().remove(
                                (BehavioralFeature) context, (Signal) handle);
                return;
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Unrecognized object " + handle
                + " or " + context);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#removeReception(java.lang.Object,
     *      java.lang.Object)
     */
    public void removeReception(Object handle, Object reception) {
        try {
            if (handle instanceof Signal && reception instanceof Reception) {
                ((Reception) reception).setSignal(null);
                return;
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("Unrecognized object " + handle
                + " or " + reception);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#addActualArgument(java.lang.Object,
     *      java.lang.Object)
     */
    public void addActualArgument(Object handle, Object argument) {
        if (handle instanceof Action && argument instanceof Argument) {
            ((Action) handle).getActualArgument().add(argument);
            return;
        }
        throw new IllegalArgumentException("Unrecognized object " + handle
                + " or " + argument);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#addClassifier(java.lang.Object,
     *      java.lang.Object)
     */
    public void addClassifier(Object handle, Object classifier) {
        if (handle instanceof Instance && classifier instanceof Classifier) {
            ((Instance) handle).getClassifier().add(classifier);
            return;
        }
        throw new IllegalArgumentException("Unrecognized object " + handle
                + " or " + classifier);
    }

    /**
     * Add a context to a Signal.
     * 
     * @param handle The signal
     * @param behavorialFeature The behavorialFeature 
     */
    private void addContext(Object handle, Object behavorialFeature) {
        if (handle instanceof Signal
                && behavorialFeature instanceof BehavioralFeature) {
            modelImpl.getUmlPackage().getCommonBehavior().
                    getAContextRaisedSignal().add(
                            (BehavioralFeature) behavorialFeature,
                            (Signal) handle);
            return;
        }
    }

    /**
     * Add a Reception to a Signal.
     * 
     * @param handle The signal
     * @param rec The Reception 
     */
    private void addReception(Object handle, Object rec) {
        if (handle instanceof Signal
                && rec instanceof Reception) {
            modelImpl.getUmlPackage().getCommonBehavior().
                    getASignalReception().add(
                            (Signal) handle,
                            (Reception) rec);
            return;
        }
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#addStimulus(java.lang.Object,
     *      java.lang.Object)
     */
    public void addStimulus(Object handle, Object stimulus) {
        if (handle != null && stimulus != null 
                && stimulus instanceof Stimulus) {
            if (handle instanceof Action) {
                ((Stimulus) stimulus).setDispatchAction((Action) handle);
                return;
            }
            if (handle instanceof Link) {
                ((Stimulus) stimulus).setCommunicationLink((Link) handle);
                return;
            }
        }
        throw new IllegalArgumentException("handle: " + handle
                + " or stimulus: " + stimulus);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setAsynchronous(java.lang.Object,
     *      boolean)
     */
    public void setAsynchronous(Object handle, boolean value) {
        if (handle instanceof Action) {
            ((Action) handle).setAsynchronous(value);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setOperation(java.lang.Object,
     *      java.lang.Object)
     */
    public void setOperation(Object handle, Object operation) {
        if (handle instanceof CallAction
                && (operation == null || operation instanceof Operation)) {
            ((CallAction) handle).setOperation((Operation) operation);
            return;
        }
        if (handle instanceof CallEvent 
                && (operation == null || operation instanceof Operation)) {
            ((CallEvent) handle).setOperation((Operation) operation);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle
                + " or operation: " + operation);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setClassifiers(java.lang.Object,
     *      java.util.Vector)
     */
    public void setClassifiers(Object handle, Vector v) {
        if (handle instanceof Instance) {
            Collection actualClassifiers = Model.getFacade().getClassifiers(
                    handle);
            if (!actualClassifiers.isEmpty()) {
                Vector classifiers = new Vector();
                classifiers.addAll(actualClassifiers);
                Iterator toRemove = classifiers.iterator();
                while (toRemove.hasNext())
                    removeClassifier(handle, toRemove.next());
            }
            if (!v.isEmpty()) {
                Iterator toAdd = v.iterator();
                while (toAdd.hasNext())
                    addClassifier(handle, toAdd.next());
            }
            return;
        }
        throw new IllegalArgumentException("handle: " + handle);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setCommunicationLink(java.lang.Object,
     *      java.lang.Object)
     */
    public void setCommunicationLink(Object handle, Object c) {
        if (handle instanceof Stimulus && c instanceof Link) {
            ((Stimulus) handle).setCommunicationLink((Link) c);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + " or c: " + c);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setComponentInstance(java.lang.Object,
     *      java.lang.Object)
     */
    public void setComponentInstance(Object handle, Object c) {
        if (handle instanceof Instance
                && (c == null || c instanceof ComponentInstance)) {
            ((Instance) handle).setComponentInstance((ComponentInstance) c);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + " or c: " + c);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setContexts(java.lang.Object,
     *      java.util.Collection)
     */
    public void setContexts(Object handle, Collection c) {
        if (handle instanceof Signal) {
            Collection actualContexts = Model.getFacade().getContexts(handle);
            if (!actualContexts.isEmpty()) {
                Vector contexts = new Vector();
                contexts.addAll(actualContexts);
                Iterator toRemove = contexts.iterator();
                while (toRemove.hasNext())
                    removeContext(handle, toRemove.next());
            }
            if (!c.isEmpty()) {
                Iterator toAdd = c.iterator();
                while (toAdd.hasNext())
                    addContext(handle, toAdd.next());
            }
            return;
        }
        throw new IllegalArgumentException("handle: " + handle);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setDispatchAction(java.lang.Object,
     *      java.lang.Object)
     */
    public void setDispatchAction(Object handle, Object value) {
        if (handle instanceof Stimulus
                && (value == null || value instanceof Action)) {
            ((Stimulus) handle).setDispatchAction((Action) value);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + " or value: "
                + value);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setInstance(java.lang.Object,
     *      java.lang.Object)
     */
    public void setInstance(Object handle, Object inst) {
        if (inst == null || inst instanceof Instance) {
            if (handle instanceof LinkEnd) {
                ((LinkEnd) handle).setInstance((Instance) inst);
                return;
            }
            if (handle instanceof AttributeLink) {
                ((AttributeLink) handle).setInstance((Instance) inst);
                return;
            }
        }
        throw new IllegalArgumentException("handle: " + handle + " or inst: "
                + inst);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setNodeInstance(java.lang.Object,
     *      java.lang.Object)
     */
    public void setNodeInstance(Object handle, Object nodeInstance) {
        if (handle instanceof ComponentInstance
                && nodeInstance instanceof NodeInstance) {
            ((ComponentInstance) handle).
                    setNodeInstance((NodeInstance) nodeInstance);
            return;
        } else if (handle instanceof ComponentInstance 
                && nodeInstance == null) {
            // TODO: Check if this is ok (this is literally adapted from NSUML)
            ((ComponentInstance) handle).setNodeInstance(null);
            return;
        }

        throw new IllegalArgumentException("handle: " + handle
                + " or nodeInstance: " + nodeInstance);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setReceiver(java.lang.Object,
     *      java.lang.Object)
     */
    public void setReceiver(Object handle, Object receiver) {
        if (handle instanceof Message
                && (receiver instanceof ClassifierRole || receiver == null)) {
            ((Message) handle).setReceiver((ClassifierRole) receiver);
            return;
        }
        if (handle instanceof Stimulus && receiver instanceof Instance) {
            ((Stimulus) handle).setReceiver((Instance) receiver);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle
                + " or receiver: " + receiver);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setReception(java.lang.Object, 
     * java.util.Collection)
     */
    public void setReception(Object handle, Collection c) {
        if (handle instanceof Signal) {
            Collection actualReceptions = 
                Model.getFacade().getReceptions(handle);
            if (!actualReceptions.isEmpty()) {
                Vector receptions = new Vector();
                receptions.addAll(actualReceptions);
                Iterator toRemove = receptions.iterator();
                while (toRemove.hasNext())
                    removeReception(handle, toRemove.next());
            }
            if (!c.isEmpty()) {
                Iterator toAdd = c.iterator();
                while (toAdd.hasNext())
                    addReception(handle, toAdd.next());
            }
            return;
        }
        throw new IllegalArgumentException("handle: " + handle);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setRecurrence(java.lang.Object,
     *      java.lang.Object)
     */
    public void setRecurrence(Object handle, Object expr) {
        if (handle instanceof Action && expr instanceof IterationExpression) {
            ((Action) handle).setRecurrence((IterationExpression) expr);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + " or expr: "
                + expr);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setScript(java.lang.Object,
     *      java.lang.Object)
     */
    public void setScript(Object handle, Object expr) {
        if (handle instanceof Action
                && (expr == null || expr instanceof ActionExpression)) {
            ((Action) handle).setScript((ActionExpression) expr);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + " or expr: "
                + expr);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setSender(java.lang.Object,
     *      java.lang.Object)
     */
    public void setSender(Object handle, Object sender) {
        if (handle instanceof Message
                && (sender instanceof ClassifierRole || sender == null)) {
            ((Message) handle).setSender((ClassifierRole) sender);
            return;
        }
        if (handle instanceof Stimulus && sender instanceof Instance) {
            ((Stimulus) handle).setSender((Instance) sender);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + " or sender: "
                + sender);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setSignal(java.lang.Object,
     *      java.lang.Object)
     */
    public void setSignal(Object handle, Object signal) {
        if (signal == null || signal instanceof Signal) {
            if (handle instanceof SendAction) {
                ((SendAction) handle).setSignal((Signal) signal);
                return;
            }
            if (handle instanceof Reception) {
                ((Reception) handle).setSignal((Signal) signal);
                return;
            }
            if (handle instanceof SignalEvent) {
                ((SignalEvent) handle).setSignal((Signal) signal);
                return;
            }
        }
        throw new IllegalArgumentException("handle: " + handle + " or signal: "
                + signal);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setSpecification(java.lang.Object,
     *      java.lang.String)
     */
    public void setSpecification(Object handle, String specification) {
        if (handle instanceof Reception) {
            ((Reception) handle).setSpecification(specification);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setTarget(java.lang.Object,
     *      java.lang.Object)
     */
    public void setTarget(Object handle, Object element) {
        if (handle instanceof Action 
                && element instanceof ObjectSetExpression) {
            ((Action) handle).setTarget((ObjectSetExpression) element);
            return;
        }
        if (handle instanceof Transition && element instanceof StateVertex) {
            ((Transition) handle).setTarget((StateVertex) element);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle
                + " or element: " + element);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setTransition(java.lang.Object,
     *      java.lang.Object)
     */
    public void setTransition(Object handle, Object trans) {
        if (trans instanceof Transition) {
            if (handle instanceof Guard) {
                ((Guard) handle).setTransition((Transition) trans);
                return;
            }
            if (handle instanceof Action) {
                ((Transition) trans).setEffect((Action) handle);
                return;
            }
        }
        throw new IllegalArgumentException("handle: " + handle + " or trans: "
                + trans);
    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#setValue(java.lang.Object,
     *      java.lang.Object)
     */
    public void setValue(Object handle, Object value) {
        if (handle instanceof Argument) {
            ((Argument) handle).setValue((Expression) value);
            return;
        }
        if (handle instanceof AttributeLink) {
            ((AttributeLink) handle).setValue((Instance) value);
            return;
        }
        if (handle instanceof TaggedValue && value instanceof String) {
            modelImpl.getExtensionMechanismsHelper().setValueOfTag(handle,
                    (String) value);
            return;
        }
        throw new IllegalArgumentException("handle: " + handle + ", value:"
                + value);
    }

    /*
     * @see CommonBehaviorHelper#getInstantiation(Object)
     */
    public Object getInstantiation(Object createaction) {
        try {
            if (createaction instanceof CreateAction) {
                return ((CreateAction) createaction).getInstantiation();
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        throw new IllegalArgumentException("handle: " + createaction);
    }

    /*
     * @see CommonBehaviorHelper#setInstantiation(Object, Object)
     */
    public void setInstantiation(Object createaction, Object instantiation) {
        if (createaction instanceof CreateAction) {
            if (instantiation instanceof Classifier) {
                ((CreateAction) createaction).setInstantiation(
                        (Classifier) instantiation);
                return;
            }
            if (instantiation == null)
                ((CreateAction) createaction).setInstantiation(null);
            return;
        }
        throw new IllegalArgumentException("handle: " + createaction
                + ", value:" + instantiation);

    }

    /*
     * @see org.argouml.model.CommonBehaviorHelper#getActionOwner(java.lang.Object)
     */
    public Object getActionOwner(Object action) {
        if (!(action instanceof Action)) {
            throw new IllegalArgumentException();
        }

        try {
            if (Model.getFacade().getStimuli(action) != null) {
                Iterator iter = Model.getFacade().getStimuli(action).iterator();
                if (iter.hasNext()) {
                    return iter.next();
                }
            }
            if (Model.getFacade().getMessages(action) != null) {
                Iterator iter = 
                    Model.getFacade().getMessages(action).iterator();
                if (iter.hasNext()) {
                    return iter.next();
                }
            }
            if (Model.getFacade().getTransition(action) != null) {
                return Model.getFacade().getTransition(action);
            }
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
        return null;
    }

    public void addAction(Object handle, Object action) {
        if (!(handle instanceof ActionSequence) 
                || !(action instanceof Action)) {
            throw new IllegalArgumentException();
        }
        try {
            ((ActionSequence) handle).getAction().add(action);
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
    }

    public void addAction(Object handle, int position, Object action) {
        if (!(handle instanceof ActionSequence) 
                || !(action instanceof Action)) {
            throw new IllegalArgumentException();
        }
        try {
            ((ActionSequence) handle).getAction().add(position, action);
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        } 
    }

    public void removeAction(Object handle, Object action) {
        if (!(handle instanceof ActionSequence) 
                || !(action instanceof Action)) {
            throw new IllegalArgumentException();
        }
        try {
            ((ActionSequence) handle).getAction().remove(action);
        } catch (InvalidObjectException e) {
            throw new InvalidElementException(e);
        }
    }
    
}
