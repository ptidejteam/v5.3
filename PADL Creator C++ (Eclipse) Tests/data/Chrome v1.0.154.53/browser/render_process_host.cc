// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.

#include "chrome/browser/render_process_host.h"

#include <windows.h>
#include <wininet.h>
#include <algorithm>
#include <sstream>
#include <vector>

#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/shared_event.h"
#include "base/shared_memory.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/thread.h"
#include "base/win_util.h"
#include "chrome/app/result_codes.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cache_manager_host.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/render_widget_helper.h"
#include "chrome/browser/render_view_host.h"
#include "chrome/browser/renderer_security_policy.h"
#include "chrome/browser/resource_message_filter.h"
#include "chrome/browser/sandbox_policy.h"
#include "chrome/browser/spellchecker.h"
#include "chrome/browser/visitedlink_master.h"
#include "chrome/browser/greasemonkey_master.h"
#include "chrome/browser/web_contents.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/win_util.h"
#include "chrome/renderer/render_process.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_util.h"
#include "sandbox/src/sandbox.h"

#include "SkBitmap.h"

#include "generated_resources.h"

namespace {

unsigned int GetMaxRendererProcessCount() {
  // Defines the maximum number of renderer processes according to the amount
  // of installed memory as reported by the OS. The table values are calculated
  // by assuming that you want the renderers to use half of the installed ram
  // and assuming that each tab uses ~25MB.
  static const int kMaxRenderersByRamTier[] = {
    4,                                  // less than 256MB
    8,                                  // 256MB
    12,                                 // 512MB
    16,                                 // 768MB
  };

  static unsigned int max_count = 0;
  if (!max_count) {
    int memory_tier = base::SysInfo::AmountOfPhysicalMemoryMB() / 256;
    if (memory_tier >= arraysize(kMaxRenderersByRamTier))
      max_count = chrome::kMaxRendererProcessCount;
    else
      max_count = kMaxRenderersByRamTier[memory_tier];
  }
  return max_count;
}

// ----------------------------------------------------------------------------

class RendererMainThread : public base::Thread {
 public:
  explicit RendererMainThread(const std::wstring& channel_id)
      : base::Thread("Chrome_InProcRendererThread"),
        channel_id_(channel_id) {
  }

 protected:
  virtual void Init() {
    CoInitialize(NULL);

    bool rv = RenderProcess::GlobalInit(channel_id_);
    DCHECK(rv);
    // It's a little lame to manually set this flag.  But the single process
    // RendererThread will receive the WM_QUIT.  We don't need to assert on
    // this thread, so just force the flag manually.
    // If we want to avoid this, we could create the InProcRendererThread
    // directly with _beginthreadex() rather than using the Thread class.
    base::Thread::SetThreadWasQuitProperly(true);
  }

  virtual void CleanUp() {
    RenderProcess::GlobalCleanup();

    CoUninitialize();
  }

 private:
  std::wstring channel_id_;
};

// Used for a View_ID where the renderer has not been attached yet
const int32 kInvalidViewID = -1;

// the global list of all renderer processes
IDMap<RenderProcessHost> all_hosts;

// Get the path to the renderer executable, which is the same as the
// current executable.
bool GetRendererPath(std::wstring* cmd_line) {
  return PathService::Get(base::FILE_EXE, cmd_line);
}

const wchar_t* const kDesktopName = L"ChromeRendererDesktop";

}  // namespace

//------------------------------------------------------------------------------

bool RenderProcessHost::run_renderer_in_process_ = false;

// static
void RenderProcessHost::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kStartRenderersManually, false);
}

// static
RenderProcessHost* RenderProcessHost::FromID(int render_process_id) {
  return all_hosts.Lookup(render_process_id);
}

RenderProcessHost::RenderProcessHost(Profile* profile)
    : profile_(profile),
      max_page_id_(-1),
      visible_widgets_(0),
      backgrounded_(true),
      notified_termination_(false) {
  host_id_ = all_hosts.Add(this);
  DCHECK(host_id_ >= 0);  // We use a negative host_id_ in destruction.
  widget_helper_ = new RenderWidgetHelper(host_id_);

  CacheManagerHost::GetInstance()->Add(host_id_);
  RendererSecurityPolicy::GetInstance()->Add(host_id_);

  PrefService* prefs = profile->GetPrefs();
  prefs->AddPrefObserver(prefs::kBlockPopups, this);
  widget_helper_->set_block_popups(
      profile->GetPrefs()->GetBoolean(prefs::kBlockPopups));

  // Note: When we create the RenderProcessHost, it's technically backgrounded,
  //       because it has no visible listeners.  But the process doesn't
  //       actually exist yet, so we'll Background it later, after creation.
}

RenderProcessHost::~RenderProcessHost() {
  // Some tests hold RenderProcessHost in a scoped_ptr, so we must call
  // Unregister here as well as in response to Release().
  Unregister();

  // We may have some unsent messages at this point, but that's OK.
  channel_.reset();

  if (process_.handle() && !run_renderer_in_process_) {
    watcher_.StopWatching();
    ProcessWatcher::EnsureProcessTerminated(process_.handle());
  }

  profile_->GetPrefs()->RemovePrefObserver(prefs::kBlockPopups, this);
}

void RenderProcessHost::Unregister() {
  if (host_id_ >= 0) {
    CacheManagerHost::GetInstance()->Remove(host_id_);
    RendererSecurityPolicy::GetInstance()->Remove(host_id_);
    all_hosts.Remove(host_id_);
    host_id_ = -1;
  }
}

bool RenderProcessHost::Init() {
  // calling Init() more than once does nothing, this makes it more convenient
  // for the view host which may not be sure in some cases
  if (channel_.get())
    return true;

  // run the IPC channel on the shared IO thread.
  base::Thread* io_thread = g_browser_process->io_thread();

  scoped_refptr<ResourceMessageFilter> resource_message_filter =
      new ResourceMessageFilter(g_browser_process->resource_dispatcher_host(),
                                PluginService::GetInstance(),
                                g_browser_process->print_job_manager(),
                                host_id_,
                                profile_,
                                widget_helper_,
                                profile_->GetSpellChecker());

  CommandLine browser_command_line;

  // setup IPC channel
  std::wstring channel_id = GenerateRandomChannelID(this);
  channel_.reset(
      new IPC::SyncChannel(channel_id, IPC::Channel::MODE_SERVER, this,
                           resource_message_filter,
                           io_thread->message_loop(), true,
                           g_browser_process->shutdown_event()));
  // As a preventive mesure, we DCHECK if someone sends a synchronous message
  // with no time-out, which in the context of the browser process we should not
  // be doing.
  channel_->set_sync_messages_with_no_timeout_allowed(false);

  // build command line for renderer, we have to quote the executable name to
  // deal with spaces
  std::wstring renderer_path =
      browser_command_line.GetSwitchValue(switches::kRendererPath);
  if (renderer_path.empty())
    if (!GetRendererPath(&renderer_path))
      return false;
  std::wstring cmd_line;
  cmd_line = L"\"" + renderer_path + L"\"";
  if (logging::DialogsAreSuppressed())
    CommandLine::AppendSwitch(&cmd_line, switches::kNoErrorDialogs);

  // propagate the following switches to the renderer command line
  // (along with any associated values) if present in the browser command line
  static const wchar_t* const switch_names[] = {
    switches::kRendererAssertTest,
    switches::kRendererCrashTest,
    switches::kRendererStartupDialog,
    switches::kNoSandbox,
    switches::kTestSandbox,
    switches::kInProcessPlugins,
    switches::kDomAutomationController,
    switches::kUserAgent,
    switches::kJavaScriptFlags,
    switches::kRecordMode,
    switches::kPlaybackMode,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDumpHistogramsOnExit,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kDebugPrint,
    switches::kAllowAllActiveX,
    switches::kMemoryProfiling,
    switches::kEnableWatchdog,
    switches::kMessageLoopHistogrammer,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kDisablePopupBlocking,
    switches::kUseLowFragHeapCrt,
    switches::kGearsInRenderer,
    switches::kEnableGreasemonkey,
  };

  for (int i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      CommandLine::AppendSwitchWithValue(
          &cmd_line, switch_names[i],
          browser_command_line.GetSwitchValue(switch_names[i]));
    }
  }

  // Pass on the browser locale.
  const std::wstring locale = g_browser_process->GetApplicationLocale();
  CommandLine::AppendSwitchWithValue(&cmd_line, switches::kLang, locale);

  bool in_sandbox = !browser_command_line.HasSwitch(switches::kNoSandbox);
  if (browser_command_line.HasSwitch(switches::kInProcessPlugins)) {
    // In process plugins won't work if the sandbox is enabled.
    in_sandbox = false;
  }

  bool child_needs_help =
      DebugFlags::ProcessDebugFlags(&cmd_line,
                                    DebugFlags::RENDERER,
                                    in_sandbox);
  CommandLine::AppendSwitchWithValue(&cmd_line,
                                     switches::kProcessType,
                                     switches::kRendererProcess);

  CommandLine::AppendSwitchWithValue(&cmd_line,
                                     switches::kProcessChannelID,
                                     channel_id);

  const std::wstring& profile_path =
      browser_command_line.GetSwitchValue(switches::kUserDataDir);
  if (!profile_path.empty())
    CommandLine::AppendSwitchWithValue(&cmd_line, switches::kUserDataDir,
                                       profile_path);

  bool run_in_process = RenderProcessHost::run_renderer_in_process();
  if (run_in_process) {
    // Crank up a thread and run the initialization there.  With the way that
    // messages flow between the browser and renderer, this thread is required
    // to prevent a deadlock in single-process mode.  When using multiple
    // processes, the primordial thread in the renderer process has a message
    // loop which is used for sending messages asynchronously to the io thread
    // in the browser process.  If we don't create this thread, then the
    // RenderThread is both responsible for rendering and also for
    // communicating IO.  This can lead to deadlocks where the RenderThread is
    // waiting for the IO to complete, while the browsermain is trying to pass
    // an event to the RenderThread.
    //
    // TODO: We should consider how to better cleanup threads on exit.
    base::Thread *render_thread = new RendererMainThread(channel_id);
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    render_thread->StartWithOptions(options);
  } else {
    if (g_browser_process->local_state() &&
        g_browser_process->local_state()->GetBoolean(
            prefs::kStartRenderersManually)) {
      std::wstring message =
        L"Please start a renderer process using:\n" + cmd_line;

      // We don't know the owner window for RenderProcessHost and therefore we
      // pass a NULL HWND argument.
      win_util::MessageBox(NULL,
                           message,
                           switches::kBrowserStartRenderersManually,
                           MB_OK);
    } else {
      if (in_sandbox) {
        // spawn the child process in the sandbox
        sandbox::BrokerServices* broker_service =
            g_browser_process->broker_services();

        sandbox::ResultCode result;
        PROCESS_INFORMATION target = {0};
        sandbox::TargetPolicy* policy = broker_service->CreatePolicy();
        policy->SetJobLevel(sandbox::JOB_LOCKDOWN, 0);

        sandbox::TokenLevel initial_token = sandbox::USER_UNPROTECTED;
        if (win_util::GetWinVersion() > win_util::WINVERSION_XP) {
          // On 2003/Vista the initial token has to be restricted if the main
          // token is restricted.
          initial_token = sandbox::USER_RESTRICTED_SAME_ACCESS;
        }

        policy->SetTokenLevel(initial_token, sandbox::USER_LOCKDOWN);
        policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

        HDESK desktop = CreateDesktop(kDesktopName, NULL, NULL, 0,
                                      DESKTOP_CREATEWINDOW, NULL);
        if (desktop) {
          policy->SetDesktop(kDesktopName);
        } else {
          DLOG(WARNING) << "Failed to apply desktop security to the renderer";
        }

        if (!AddGenericPolicy(policy)) {
          NOTREACHED();
          return false;
        }

        if (!AddDllEvictionPolicy(policy)) {
          NOTREACHED();
          return false;          
        }

        result = broker_service->SpawnTarget(renderer_path.c_str(),
                                             cmd_line.c_str(),
                                             policy, &target);
        policy->Release();

        if (desktop)
          CloseDesktop(desktop);

        if (sandbox::SBOX_ALL_OK != result)
          return false;

        bool on_sandbox_desktop = (desktop != NULL);
        NotificationService::current()->Notify(
            NOTIFY_RENDERER_PROCESS_IN_SBOX, Source<RenderProcessHost>(this),
            Details<bool>(&on_sandbox_desktop));

        ResumeThread(target.hThread);
        CloseHandle(target.hThread);
        process_.set_handle(target.hProcess);

        // Help the process a little. It can't start the debugger by itself if
        // the process is in a sandbox.
        if (child_needs_help)
            DebugUtil::SpawnDebuggerOnProcess(target.dwProcessId);
      } else {
        // spawn child process
        HANDLE process;
        if (!process_util::LaunchApp(cmd_line, false, false, &process))
          return false;
        process_.set_handle(process);
      }

      watcher_.StartWatching(process_.handle(), this);
    }
  }

  // Now that the process is created, set it's backgrounding accordingly.
  SetBackgrounded(backgrounded_);

  // Send the process its initial VisitedLink and Greasemonkey data.
  HANDLE target_process = process_.handle();
  if (!target_process) {
    // Target process can be null if it's started with the --single-process
    // flag.
    target_process = GetCurrentProcess();
  }

  InitVisitedLinks(target_process);
  InitGreasemonkeyScripts(target_process);

  if (max_page_id_ != -1)
    channel_->Send(new ViewMsg_SetNextPageID(max_page_id_ + 1));

  return true;
}

void RenderProcessHost::InitVisitedLinks(HANDLE target_process) {
  VisitedLinkMaster* visitedlink_master = profile_->GetVisitedLinkMaster();
  if (!visitedlink_master) {
    return;
  }

  SharedMemoryHandle handle_for_process = NULL;
  visitedlink_master->ShareToProcess(target_process, &handle_for_process);
  DCHECK(handle_for_process);
  if (handle_for_process) {
    channel_->Send(new ViewMsg_VisitedLink_NewTable(handle_for_process));
  }
}

void RenderProcessHost::InitGreasemonkeyScripts(HANDLE target_process) {
  CommandLine command_line;
  if (!command_line.HasSwitch(switches::kEnableGreasemonkey)) {
    return;
  }

  // TODO(aa): Figure out lifetime and ownership of this object
  // - VisitedLinkMaster is owned by Profile, but there has been talk of
  //   having scripts live elsewhere besides the profile.
  // - File IO should be asynchronous (see VisitedLinkMaster), but how do we
  //   get scripts to the first renderer without blocking startup? Should we
  //   cache some information across restarts?
  GreasemonkeyMaster* greasemonkey_master =
      Singleton<GreasemonkeyMaster>::get();
  if (!greasemonkey_master) {
    return;
  }

  // TODO(aa): This does blocking IO. Move to background thread.
  greasemonkey_master->UpdateScripts();

  SharedMemoryHandle handle_for_process = NULL;
  greasemonkey_master->ShareToProcess(target_process, &handle_for_process);
  DCHECK(handle_for_process);
  if (handle_for_process) {
    channel_->Send(new ViewMsg_Greasemonkey_NewScripts(handle_for_process));
  }
}

void RenderProcessHost::Attach(IPC::Channel::Listener* listener,
                               int routing_id) {
  listeners_.AddWithID(listener, routing_id);
}

void RenderProcessHost::Release(int listener_id) {
  DCHECK(listeners_.Lookup(listener_id) != NULL);
  listeners_.Remove(listener_id);

  // make sure that all associated resource requests are stopped.
  widget_helper_->CancelResourceRequests(listener_id);

  // when no other owners of this object, we can delete ourselves
  if (listeners_.IsEmpty()) {
    if (!notified_termination_) {
      bool close_expected = true;
      NotificationService::current()->Notify(NOTIFY_RENDERER_PROCESS_TERMINATED,
                                             Source<RenderProcessHost>(this),
                                             Details<bool>(&close_expected));
      notified_termination_ = true;
    }
    Unregister();
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void RenderProcessHost::ReportExpectingClose(int32 listener_id) {
  listeners_expecting_close_.insert(listener_id);
}

bool RenderProcessHost::FastShutdownIfPossible() {
  HANDLE proc = process();
  if (!proc)
    return false;
  // If we're in single process mode, do nothing.
  if (RenderProcessHost::run_renderer_in_process())
    return false;

  // Test if there's an unload listener
  RenderProcessHost::listeners_iterator iter;
  // NOTE: This is a bit dangerous.  We know that for now, listeners are
  // always RenderWidgetHosts.  But in theory, they don't have to be.
  for (iter = listeners_begin(); iter != listeners_end(); ++iter) {
    RenderWidgetHost* widget = static_cast<RenderWidgetHost*>(iter->second);
    DCHECK(widget);
    if (!widget || !widget->IsRenderView())
      continue;
    RenderViewHost* rvh = static_cast<RenderViewHost*>(widget);
    if (rvh->HasUnloadListener()) {
      // NOTE: It's possible that an onunload listener may be installed
      // while we're shutting down, so there's a small race here.  Given that
      // the window is small, it's unlikely that the web page has much
      // state that will be lost by not calling its unload handlers properly.
      return false;
    }
  }
  // Otherwise, call TerminateProcess.  Using exit code 0 means that UMA won't
  // treat this as a renderer crash.
  ::TerminateProcess(proc, ResultCodes::NORMAL_EXIT);
  return true;
}

// Static. This function can be called from the IO Thread or from the UI thread.
void RenderProcessHost::BadMessageTerminateProcess(uint16 msg_type,
                                                   HANDLE process) {
  LOG(ERROR) << "bad message " << msg_type << " terminating renderer.";
  if (RenderProcessHost::run_renderer_in_process()) {
    // In single process mode it is better if we don't suicide but just crash.
    CHECK(false);
  }
  NOTREACHED();
  ::TerminateProcess(process, ResultCodes::KILLED_BAD_MESSAGE);
}

void RenderProcessHost::UpdateMaxPageID(int32 page_id) {
  if (page_id > max_page_id_)
    max_page_id_ = page_id;
}

void RenderProcessHost::CrossSiteClosePageACK(int new_render_process_host_id,
                                              int new_request_id) {
  widget_helper_->CrossSiteClosePageACK(new_render_process_host_id,
                                        new_request_id);
}

void RenderProcessHost::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // dispatch control messages
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(RenderProcessHost, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_PageContents, OnPageContents)
      IPC_MESSAGE_HANDLER(ViewHostMsg_UpdatedCacheStats,
                          OnUpdatedCacheStats)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      // The message had a handler, but its de-serialization failed.
      // We consider this a capital crime. Kill the renderer if we have one.
      ReceivedBadMessage(msg.type());
    }
    return;
  }

  // dispatch incoming messages to the appropriate TabContents
  IPC::Channel::Listener* listener = listeners_.Lookup(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return;
  }
  listener->OnMessageReceived(msg);
}

void RenderProcessHost::OnChannelConnected(int32 peer_pid) {
  // process_ is not NULL if we created the renderer process
  if (!process_.handle()) {
    if (GetCurrentProcessId() == peer_pid) {
      // We are in single-process mode. In theory we should have access to
      // ourself but it may happen that we don't.
      process_.set_handle(GetCurrentProcess());
    } else {
      // Request MAXIMUM_ALLOWED to match the access a handle
      // returned by CreateProcess() has to the process object.
      process_.set_handle(OpenProcess(MAXIMUM_ALLOWED, FALSE, peer_pid));
      DCHECK(process_.handle());
      watcher_.StartWatching(process_.handle(), this);
    }
  } else {
    // Need to verify that the peer_pid is actually the process we know, if
    // it is not, we need to panic now. See bug 1002150.
    CHECK(peer_pid == process_.pid());
  }
}

// indicates the renderer process has exited
void RenderProcessHost::OnObjectSignaled(HANDLE object) {
  DCHECK(process_.handle());
  DCHECK(channel_.get());
  DCHECK_EQ(object, process_.handle());

  bool clean_shutdown = !process_util::DidProcessCrash(object);

  process_.Close();

  channel_.reset();

  if (!notified_termination_) {
    // If |close_expected| is false, it means the renderer process went away
    // before the web views expected it; count it as a crash.
    NotificationService::current()->Notify(NOTIFY_RENDERER_PROCESS_TERMINATED,
                                           Source<RenderProcessHost>(this),
                                           Details<bool>(&clean_shutdown));
    notified_termination_ = true;
  }

  // This process should detach all the listeners, causing the object to be
  // deleted. We therefore need a stack copy of the web view list to avoid
  // crashing when checking for the termination condition the last time.
  IDMap<IPC::Channel::Listener> local_listeners(listeners_);
  for (IDMap<IPC::Channel::Listener>::const_iterator i = local_listeners.begin();
       i != local_listeners.end(); ++i) {
    i->second->OnMessageReceived(ViewHostMsg_RendererGone(i->first));
  }
  // at this point, this object should be deleted
}

// Used to send responses to resource requests
bool RenderProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

void RenderProcessHost::OnPageContents(const GURL& url,
                                       int32 page_id,
                                       const std::wstring& contents) {
  Profile* p = profile();
  if (!p || p->IsOffTheRecord())
    return;

  HistoryService* hs = p->GetHistoryService(Profile::IMPLICIT_ACCESS);
  if (hs)
    hs->SetPageContents(url, contents);
}

void RenderProcessHost::OnUpdatedCacheStats(
    const CacheManager::UsageStats& stats) {
  CacheManagerHost::GetInstance()->ObserveStats(host_id(), stats);
}

// static
RenderProcessHost::iterator RenderProcessHost::begin() {
  return all_hosts.begin();
}

// static
RenderProcessHost::iterator RenderProcessHost::end() {
  return all_hosts.end();
}

// static
size_t RenderProcessHost::size() {
  return all_hosts.size();
}

// Returns true if the given host is suitable for launching a new view
// associated with the given profile.
// TODO(jabdelmalek): do we want to avoid processes with hung renderers
// or with a large memory consumption?
static bool IsSuitableHost(Profile* profile, RenderProcessHost* host) {
  return host->profile() == profile;
}

// static
RenderProcessHost* RenderProcessHost::GetExistingProcessHost(Profile* profile) {
  // First figure out which existing renderers we can use
  std::vector<RenderProcessHost*> suitable_renderers;
  suitable_renderers.reserve(all_hosts.size());

  for (IDMap<RenderProcessHost>::const_iterator iter = all_hosts.begin();
       iter != all_hosts.end(); ++iter) {
    if (IsSuitableHost(profile, iter->second))
      suitable_renderers.push_back(iter->second);
  }

  // Now pick a random suitable renderer, if we have any
  if (!suitable_renderers.empty()) {
    int suitable_count = static_cast<int>(suitable_renderers.size());
    int random_index = base::RandInt(0, suitable_count - 1);
    return suitable_renderers[random_index];
  }

  return NULL;
}

void RenderProcessHost::SetBackgrounded(bool backgrounded) {
  // If the process_ is NULL, the process hasn't been created yet.
  if (process_.handle()) {
    bool rv = process_.SetProcessBackgrounded(backgrounded);
    if (!rv) {
      return;
    }

    // Now tune the memory footprint of the renderer.
    // If the OS needs to page, we'd rather it page idle renderers.
    BrowserProcess::MemoryModel model = g_browser_process->memory_model();
    if (model < BrowserProcess::HIGH_MEMORY_MODEL) {
      if (backgrounded) {
        if (model == BrowserProcess::LOW_MEMORY_MODEL)
          process_.EmptyWorkingSet();
        else if (model == BrowserProcess::MEDIUM_MEMORY_MODEL)
          process_.ReduceWorkingSet();
      } else {
        if (model == BrowserProcess::MEDIUM_MEMORY_MODEL)
          process_.UnReduceWorkingSet();
      }
    }
  }

  // Note: we always set the backgrounded_ value.  If the process is NULL
  // (and hence hasn't been created yet), we will set the process priority
  // later when we create the process.
  backgrounded_ = backgrounded;
}

void RenderProcessHost::WidgetRestored() {
  // Verify we were properly backgrounded.
  DCHECK(backgrounded_ == (visible_widgets_ == 0));
  visible_widgets_++;
  SetBackgrounded(false);
}

void RenderProcessHost::WidgetHidden() {
  // On startup, the browser will call Hide
  if (backgrounded_)
    return;

  DCHECK(backgrounded_ == (visible_widgets_ == 0));
  visible_widgets_--;
  DCHECK(visible_widgets_ >= 0);
  if (visible_widgets_ == 0) {
    DCHECK(!backgrounded_);
    SetBackgrounded(true);
  }
}

void RenderProcessHost::AddWord(const std::wstring& word) {
  base::Thread* io_thread = g_browser_process->io_thread();
  if (profile_->GetSpellChecker()) {
    io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        profile_->GetSpellChecker(), &SpellChecker::AddWord, word));
  }
}

// NotificationObserver implementation.
void RenderProcessHost::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  switch (type) {
    case NOTIFY_PREF_CHANGED: {
      std::wstring* pref_name_in = Details<std::wstring>(details).ptr();
      DCHECK(Source<PrefService>(source).ptr() == profile()->GetPrefs());
      if (*pref_name_in == prefs::kBlockPopups) {
        widget_helper_->set_block_popups(
            profile()->GetPrefs()->GetBoolean(prefs::kBlockPopups));
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

// static
bool RenderProcessHost::ShouldTryToUseExistingProcessHost() {
  unsigned int renderer_process_count =
      static_cast<unsigned int>(all_hosts.size());

  // NOTE: Sometimes it's necessary to create more render processes than
  //       GetMaxRendererProcessCount(), for instance when we want to create
  //       a renderer process for a profile that has no existing renderers.
  //       This is OK in moderation, since the GetMaxRendererProcessCount()
  //       is conservative.

  return run_renderer_in_process() ||
         (renderer_process_count >= GetMaxRendererProcessCount());
}

