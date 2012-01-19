/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.40
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package mesos;

public class MesosSchedulerDriver extends SchedulerDriver {
  private long swigCPtr;

  protected MesosSchedulerDriver(long cPtr, boolean cMemoryOwn) {
    super(mesosJNI.SWIGMesosSchedulerDriverUpcast(cPtr), cMemoryOwn);
    swigCPtr = cPtr;
  }

  protected static long getCPtr(MesosSchedulerDriver obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }

    protected void finalize() {
      synchronized (schedulers) {
        schedulers.remove(getScheduler());
      }
      delete();
    }
  
  public synchronized void delete() {
    if (swigCPtr != 0) {
      if (swigCMemOwn) {
        swigCMemOwn = false;
        mesosJNI.delete_MesosSchedulerDriver(swigCPtr);
      }
      swigCPtr = 0;
    }
    super.delete();
  }

    private static java.util.HashSet<Scheduler> schedulers =
      new java.util.HashSet<Scheduler>();

    private static long getCPtrAndAddReference(Scheduler scheduler) {
      synchronized (schedulers) {
        schedulers.add(scheduler);
      }
      return Scheduler.getCPtr(scheduler);
    }
  
  public MesosSchedulerDriver(Scheduler sched, String url, String fid) {
    this(mesosJNI.new_MesosSchedulerDriver__SWIG_0(getCPtrAndAddReference(sched), sched, url, fid), true);
  }

  public MesosSchedulerDriver(Scheduler sched, String url) {
    this(mesosJNI.new_MesosSchedulerDriver__SWIG_1(getCPtrAndAddReference(sched), sched, url), true);
  }

  public MesosSchedulerDriver(Scheduler sched, java.util.Map<String, String> params, String fid) {
    this(mesosJNI.new_MesosSchedulerDriver__SWIG_2(getCPtrAndAddReference(sched), sched, params, fid), true);
  }

  public MesosSchedulerDriver(Scheduler sched, java.util.Map<String, String> params) {
    this(mesosJNI.new_MesosSchedulerDriver__SWIG_3(getCPtrAndAddReference(sched), sched, params), true);
  }

  public int start() {
    return mesosJNI.MesosSchedulerDriver_start(swigCPtr, this);
  }

  public int stop() {
    return mesosJNI.MesosSchedulerDriver_stop(swigCPtr, this);
  }

  public int join() {
    return mesosJNI.MesosSchedulerDriver_join(swigCPtr, this);
  }

  public int run() {
    return mesosJNI.MesosSchedulerDriver_run(swigCPtr, this);
  }

  public int sendFrameworkMessage(FrameworkMessage message) {
    return mesosJNI.MesosSchedulerDriver_sendFrameworkMessage(swigCPtr, this, FrameworkMessage.getCPtr(message), message);
  }

  public int killTask(int tid) {
    return mesosJNI.MesosSchedulerDriver_killTask(swigCPtr, this, tid);
  }

  public int replyToOffer(String offerId, java.util.List<TaskDescription> task, java.util.Map<String, String> params) {
    return mesosJNI.MesosSchedulerDriver_replyToOffer(swigCPtr, this, offerId, task, params);
  }

  public int reviveOffers() {
    return mesosJNI.MesosSchedulerDriver_reviveOffers(swigCPtr, this);
  }

  public int sendHints(java.util.Map<String, String> hints) {
    return mesosJNI.MesosSchedulerDriver_sendHints(swigCPtr, this, hints);
  }

  public Scheduler getScheduler() {
    long cPtr = mesosJNI.MesosSchedulerDriver_getScheduler(swigCPtr, this);
    return (cPtr == 0) ? null : new Scheduler(cPtr, false);
  }

}