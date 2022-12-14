// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_process_host.h"

#include <windows.h>
#include <vector>

#include "base/command_line.h"
#include "base/debug_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_browsing_context.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/plugin_process_info.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/resource_dispatcher_host.h"
#include "chrome/browser/sandbox_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/process_watcher.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/win_util.h"
#include "net/base/cookie_monster.h"
#include "net/url_request/url_request.h"
#include "sandbox/src/sandbox.h"

static const char kDefaultPluginFinderURL[] =
    "http://dl.google.com/chrome/plugins/plugins2.xml";

// The NotificationTask is used to notify about plugin process connection/
// disconnection. It is needed because the notifications in the
// NotificationService must happen in the main thread.
class PluginNotificationTask : public Task {
 public:
  PluginNotificationTask(NotificationType notification_type,
                         std::wstring dll_path,
                         HANDLE process);

  virtual void Run();

 private:
  NotificationType notification_type_;
  std::wstring dll_path_;
  HANDLE process_;
};

PluginNotificationTask::PluginNotificationTask(
    NotificationType notification_type,
    std::wstring dll_path,
    HANDLE process)
    : notification_type_(notification_type),
      process_(process),
      dll_path_(dll_path) {
}

void PluginNotificationTask::Run() {
  // Verify that the notification type is one that makes sense.
  switch (notification_type_) {
    case NOTIFY_PLUGIN_PROCESS_HOST_CONNECTED:
    case NOTIFY_PLUGIN_PROCESS_HOST_DISCONNECTED:
    case NOTIFY_PLUGIN_PROCESS_CRASHED:
    case NOTIFY_PLUGIN_INSTANCE_CREATED:
      break;

    default:
      NOTREACHED();
      return;
  }

  PluginProcessInfo ppi(dll_path_, process_);
  // As mentioned in the notification_types.h, the PluginProcessInfo details
  // are only valid for the time of the notification.
  NotificationService::current()->
      Notify(notification_type_, NotificationService::AllSources(),
             Details<PluginProcessInfo>(&ppi));
}

// The PluginDownloadUrlHelper is used to handle one download URL request
// from the plugin. Each download request is handled by a new instance
// of this class.
class PluginDownloadUrlHelper : public URLRequest::Delegate {
  static const int kDownloadFileBufferSize = 32768;
 public:
  PluginDownloadUrlHelper(const std::string& download_url,
                          int source_pid, HWND caller_window);
  ~PluginDownloadUrlHelper();

  void InitiateDownload();

  // URLRequest::Delegate
  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url);
  virtual void OnAuthRequired(URLRequest* request,
                              net::AuthChallengeInfo* auth_info);
  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert);
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int bytes_read);

  void OnDownloadCompleted(URLRequest* request);

 protected:
  void DownloadCompletedHelper(bool success);

  // The download file request initiated by the plugin.
  URLRequest* download_file_request_;
  // Handle to the downloaded file.
  HANDLE download_file_;
  // The full path of the downloaded file.
  std::wstring download_file_path_;
  // The buffer passed off to URLRequest::Read.
  char download_file_buffer_[kDownloadFileBufferSize];
  // The window handle for sending the WM_COPYDATA notification,
  // indicating that the download completed.
  HWND download_file_caller_window_;

  std::string download_url_;
  int download_source_pid_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginDownloadUrlHelper);
};

PluginDownloadUrlHelper::PluginDownloadUrlHelper(
    const std::string& download_url,
    int source_pid, HWND caller_window)
    : download_url_(download_url),
      download_file_caller_window_(caller_window),
      download_source_pid_(source_pid),
      download_file_request_(NULL),
      download_file_(INVALID_HANDLE_VALUE) {
  DCHECK(::IsWindow(caller_window));
  memset(download_file_buffer_, 0, arraysize(download_file_buffer_));
}

PluginDownloadUrlHelper::~PluginDownloadUrlHelper() {
  if (download_file_request_) {
    delete download_file_request_;
    download_file_request_ = NULL;
  }

  if (download_file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(INVALID_HANDLE_VALUE);
    download_file_ = NULL;
  }
}

void PluginDownloadUrlHelper::InitiateDownload() {
  download_file_request_= new URLRequest(GURL(download_url_), this);
  download_file_request_->set_origin_pid(download_source_pid_);
  download_file_request_->set_context(Profile::GetDefaultRequestContext());
  download_file_request_->Start();
}

void PluginDownloadUrlHelper::OnReceivedRedirect(URLRequest* request,
                                                 const GURL& new_url) {
}

void PluginDownloadUrlHelper::OnAuthRequired(
    URLRequest* request,
    net::AuthChallengeInfo* auth_info) {
  URLRequest::Delegate::OnAuthRequired(request, auth_info);
  DownloadCompletedHelper(false);
}

void PluginDownloadUrlHelper::OnSSLCertificateError(URLRequest* request,
                                                    int cert_error,
                                                    net::X509Certificate* cert) {
  URLRequest::Delegate::OnSSLCertificateError(request, cert_error, cert);
  DownloadCompletedHelper(false);
}

void PluginDownloadUrlHelper::OnResponseStarted(URLRequest* request) {
  if (download_file_ == INVALID_HANDLE_VALUE) {
    file_util::GetTempDir(&download_file_path_);
    download_file_path_ += L"\\";

    GURL request_url = request->url();
    download_file_path_ += UTF8ToWide(request_url.ExtractFileName());

    download_file_ = CreateFile(download_file_path_.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0, NULL, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (download_file_ == INVALID_HANDLE_VALUE) {
      NOTREACHED();
      OnDownloadCompleted(request);
      return;
    }
  }
  if (!request->status().is_success()) {
    OnDownloadCompleted(request);
  } else {
    // Initiate a read.
    int bytes_read = 0;
    if (!request->Read(download_file_buffer_, arraysize(download_file_buffer_),
                       &bytes_read)) {
      // If the error is not an IO pending, then we're done
      // reading.
      if (!request->status().is_io_pending()) {
        OnDownloadCompleted(request);
      }
    } else if (bytes_read == 0) {
      OnDownloadCompleted(request);
    } else {
      OnReadCompleted(request, bytes_read);
    }
  }
}

void PluginDownloadUrlHelper::OnReadCompleted(URLRequest* request,
                                              int bytes_read) {
  DCHECK(download_file_ != INVALID_HANDLE_VALUE);

  if (bytes_read == 0) {
    OnDownloadCompleted(request);
    return;
  }

  int request_bytes_read = bytes_read;

  while (request->status().is_success()) {
    unsigned long bytes_written = 0;
    BOOL write_result = WriteFile(download_file_, download_file_buffer_,
                                  request_bytes_read, &bytes_written, NULL);
    DCHECK(!write_result || (bytes_written == request_bytes_read));

    if (!write_result || (bytes_written != request_bytes_read)) {
      DownloadCompletedHelper(false);
      break;
    }

    // Start reading
    request_bytes_read = 0;
    if (!request->Read(download_file_buffer_, arraysize(download_file_buffer_),
                       &request_bytes_read)) {
      if (!request->status().is_io_pending()) {
        // If the error is not an IO pending, then we're done
        // reading.
        OnDownloadCompleted(request);
      }
      break;
    } else if (request_bytes_read == 0) {
      OnDownloadCompleted(request);
      break;
    }
  }
}

void PluginDownloadUrlHelper::OnDownloadCompleted(URLRequest* request) {
  bool success = true;
  if (!request->status().is_success()) {
    success = false;
  } else if (download_file_ == INVALID_HANDLE_VALUE) {
    success = false;
  }

  DownloadCompletedHelper(success);
}

void PluginDownloadUrlHelper::DownloadCompletedHelper(bool success) {
  if (download_file_ != INVALID_HANDLE_VALUE) {
    ::CloseHandle(download_file_);
    download_file_ = INVALID_HANDLE_VALUE;
  }

  COPYDATASTRUCT download_file_data = {0};
  download_file_data.cbData =
      static_cast<unsigned long>((download_file_path_.length() + 1) *
                                  sizeof(wchar_t));
  download_file_data.lpData =
      const_cast<wchar_t *>(download_file_path_.c_str());
  download_file_data.dwData = success;

  if (::IsWindow(download_file_caller_window_)) {
    ::SendMessage(download_file_caller_window_, WM_COPYDATA, NULL,
                  reinterpret_cast<LPARAM>(&download_file_data));
  }
  // Don't access any members after this.
  delete this;
}

PluginProcessHost::PluginProcessHost(PluginService* plugin_service)
    : process_(NULL),
      opening_channel_(false),
      resource_dispatcher_host_(plugin_service->resource_dispatcher_host()),
      plugin_service_(plugin_service) {
  DCHECK(resource_dispatcher_host_);
}

PluginProcessHost::~PluginProcessHost() {
  if (process_.handle()) {
    watcher_.StopWatching();
    ProcessWatcher::EnsureProcessTerminated(process_.handle());
  }
}

bool PluginProcessHost::Init(const std::wstring& dll,
                             const std::string& activex_clsid,
                             const std::wstring& locale) {
  DCHECK(channel_.get() == NULL);

  dll_path_ = dll;
  channel_id_ = GenerateRandomChannelID(this);
  channel_.reset(new IPC::Channel(channel_id_,
                                  IPC::Channel::MODE_SERVER,
                                  this));
  if (!channel_->Connect())
    return false;

  // build command line for plugin, we have to quote the dll path to deal with
  // spaces.
  std::wstring plugin_path;
  if (!PathService::Get(base::FILE_EXE, &plugin_path))
    return false;

  std::wstring cmd_line(L"\"");
  cmd_line += plugin_path;
  cmd_line += L"\"";
  if (logging::DialogsAreSuppressed())
    CommandLine::AppendSwitch(&cmd_line, switches::kNoErrorDialogs);

  CommandLine browser_command_line;

  // propagate the following switches to the plugin command line (along with
  // any associated values) if present in the browser command line
  static const wchar_t* const switch_names[] = {
    switches::kPluginStartupDialog,
    switches::kNoSandbox,
    switches::kSafePlugins,
    switches::kTestSandbox,
    switches::kUserAgent,
    switches::kDisableBreakpad,
    switches::kFullMemoryCrashReport,
    switches::kEnableLogging,
    switches::kDisableLogging,
    switches::kLoggingLevel,
    switches::kUserDataDir,
    switches::kAllowAllActiveX,
    switches::kEnableDCHECK,
    switches::kSilentDumpOnDCHECK,
    switches::kMemoryProfiling,
    switches::kUseLowFragHeapCrt,
  };

  for (int i = 0; i < arraysize(switch_names); ++i) {
    if (browser_command_line.HasSwitch(switch_names[i])) {
      CommandLine::AppendSwitchWithValue(
          &cmd_line, switch_names[i],
          browser_command_line.GetSwitchValue(switch_names[i]));
    }
  }

  // If specified, prepend a launcher program to the command line.
  std::wstring plugin_launcher =
      browser_command_line.GetSwitchValue(switches::kPluginLauncher);
  if (!plugin_launcher.empty())
    cmd_line = plugin_launcher + L" " + cmd_line;

  if (!locale.empty()) {
    // Pass on the locale so the null plugin will use the right language in the
    // prompt to install the desired plugin.
    CommandLine::AppendSwitchWithValue(&cmd_line, switches::kLang, locale);
  }

  // Gears requires the data dir to be available on startup.
  std::wstring data_dir = plugin_service_->GetChromePluginDataDir();;
  DCHECK(!data_dir.empty());
  CommandLine::AppendSwitchWithValue(&cmd_line, 
                                     switches::kPluginDataDir,
				     data_dir);

  CommandLine::AppendSwitchWithValue(&cmd_line,
                                     switches::kProcessType,
                                     switches::kPluginProcess);

  CommandLine::AppendSwitchWithValue(&cmd_line,
                                     switches::kProcessChannelID,
                                     channel_id_);

  CommandLine::AppendSwitchWithValue(&cmd_line,
                                     switches::kPluginPath,
                                     dll);

  bool in_sandbox = !browser_command_line.HasSwitch(switches::kNoSandbox) &&
                    browser_command_line.HasSwitch(switches::kSafePlugins);

  bool child_needs_help =
      DebugFlags::ProcessDebugFlags(&cmd_line, DebugFlags::PLUGIN, in_sandbox);

  if (in_sandbox) {
    // spawn the child process in the sandbox
    sandbox::BrokerServices* broker_service =
        g_browser_process->broker_services();

    sandbox::ResultCode result;
    PROCESS_INFORMATION target = {0};
    sandbox::TargetPolicy* policy = broker_service->CreatePolicy();

    std::wstring trusted_plugins =
        browser_command_line.GetSwitchValue(switches::kTrustedPlugins);
    if (!AddPolicyForPlugin(dll, activex_clsid, trusted_plugins, policy)) {
      NOTREACHED();
      return false;
    }

    if (!AddGenericPolicy(policy)) {
      NOTREACHED();
      return false;
    }

    result = broker_service->SpawnTarget(plugin_path.c_str(),
                                         cmd_line.c_str(), policy, &target);
    policy->Release();
    if (sandbox::SBOX_ALL_OK != result)
      return false;

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

  std::wstring gears_path;
  std::wstring dll_lc = dll;
  if (PathService::Get(chrome::FILE_GEARS_PLUGIN, &gears_path)) {
    StringToLowerASCII(&gears_path);
    StringToLowerASCII(&dll_lc);
    if (dll_lc == gears_path) {
      // Give Gears plugins "background" priority.  See
      // http://b/issue?id=1280317.
      process_.SetProcessBackgrounded(true);
    }
  }

  opening_channel_ = true;

  return true;
}

bool PluginProcessHost::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }
  return channel_->Send(msg);
}

// indicates the plugin process has exited
void PluginProcessHost::OnObjectSignaled(HANDLE object) {
  DCHECK(process_.handle());
  DCHECK_EQ(object, process_.handle());

  bool did_crash = process_util::DidProcessCrash(object);

  if (did_crash) {
    // Report that this plugin crashed.
    plugin_service_->main_message_loop()->PostTask(FROM_HERE,
        new PluginNotificationTask(NOTIFY_PLUGIN_PROCESS_CRASHED,
                                   dll_path(), object));
  }
  // Notify in the main loop of the disconnection.
  plugin_service_->main_message_loop()->PostTask(FROM_HERE,
      new PluginNotificationTask(NOTIFY_PLUGIN_PROCESS_HOST_DISCONNECTED,
                                 dll_path(), object));

  // Cancel all requests for plugin processes.
  // TODO(mpcomplete): use a real process ID when http://b/issue?id=1210062 is
  // fixed.
  resource_dispatcher_host_->CancelRequestsForProcess(-1);

  // This next line will delete this. It should be kept at the end of the
  // method.
  plugin_service_->OnPluginProcessExited(this);
}


void PluginProcessHost::OnMessageReceived(const IPC::Message& msg) {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging* logger = IPC::Logging::current();
  if (msg.type() == IPC_LOGGING_ID) {
    logger->OnReceivedLoggingMessage(msg);
    return;
  }
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPreDispatchMessage(msg);
#endif

  IPC_BEGIN_MESSAGE_MAP(PluginProcessHost, msg)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ChannelCreated, OnChannelCreated)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_GetPluginFinderUrl,
                        OnGetPluginFinderUrl)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ShutdownRequest,
                        OnPluginShutdownRequest)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_PluginMessage, OnPluginMessage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestResource, OnRequestResource)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DataReceived_ACK, OnDataReceivedACK)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UploadProgress_ACK, OnUploadProgressACK)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_SyncLoad, OnSyncLoad)
    IPC_MESSAGE_HANDLER(PluginProcessHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

#ifdef IPC_MESSAGE_LOG_ENABLED
  if (logger->Enabled())
    logger->OnPostDispatchMessage(msg, channel_id_);
#endif
}

void PluginProcessHost::OnChannelConnected(int32 peer_pid) {
  opening_channel_ = false;

  for (size_t i = 0; i < pending_requests_.size(); ++i) {
    RequestPluginChannel(pending_requests_[i].renderer_message_filter_.get(),
                         pending_requests_[i].mime_type,
                         pending_requests_[i].reply_msg);
  }

  pending_requests_.clear();

  // Notify in the main loop of the connection.
  plugin_service_->main_message_loop()->PostTask(FROM_HERE,
      new PluginNotificationTask(NOTIFY_PLUGIN_PROCESS_HOST_CONNECTED,
                                 dll_path(), process()));
}

void PluginProcessHost::OnChannelError() {
  opening_channel_ = false;
  for (size_t i = 0; i < pending_requests_.size(); ++i) {
    ReplyToRenderer(pending_requests_[i].renderer_message_filter_.get(),
                    std::wstring(),
                    std::wstring(),
                    pending_requests_[i].reply_msg);
  }

  pending_requests_.clear();
}

void PluginProcessHost::OpenChannelToPlugin(
    ResourceMessageFilter* renderer_message_filter,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  // Notify in the main loop of the instantiation.
  plugin_service_->main_message_loop()->PostTask(FROM_HERE,
      new PluginNotificationTask(NOTIFY_PLUGIN_INSTANCE_CREATED,
                                 dll_path(), process()));

  if (opening_channel_) {
    pending_requests_.push_back(
        ChannelRequest(renderer_message_filter, mime_type, reply_msg));
    return;
  }

  if (!channel_.get()) {
    // There was an error opening the channel, tell the renderer.
    ReplyToRenderer(renderer_message_filter, std::wstring(), std::wstring(),
                    reply_msg);
    return;
  }

  // We already have an open channel, send a request right away to plugin.
  RequestPluginChannel(renderer_message_filter, mime_type, reply_msg);
}

void PluginProcessHost::OnRequestResource(
    const IPC::Message& message,
    int request_id,
    const ViewHostMsg_Resource_Request& request) {
  // TODO(mpcomplete): we need a "process_id" mostly for a unique identifier.
  // We should decouple the idea of a render_process_host_id from the unique ID
  // in ResourceDispatcherHost.
  int render_process_host_id = -1;
  URLRequestContext* context = CPBrowsingContextManager::Instance()->
        ToURLRequestContext(request.request_context);
  // TODO(mpcomplete): remove fallback case when Gears support is prevalent.
  if (!context)
    context = Profile::GetDefaultRequestContext();

  resource_dispatcher_host_->BeginRequest(this,
                                          process_.handle(),
                                          render_process_host_id,
                                          MSG_ROUTING_CONTROL,
                                          request_id,
                                          request,
                                          context,
                                          NULL);
}

void PluginProcessHost::OnCancelRequest(int request_id) {
  int render_process_host_id = -1;
  resource_dispatcher_host_->CancelRequest(render_process_host_id,
                                           request_id, true);
}

void PluginProcessHost::OnDataReceivedACK(int request_id) {
  int render_process_host_id = -1;
  resource_dispatcher_host_->OnDataReceivedACK(render_process_host_id,
                                               request_id);
}

void PluginProcessHost::OnUploadProgressACK(int request_id) {
  int render_process_host_id = -1;
  resource_dispatcher_host_->OnUploadProgressACK(render_process_host_id,
                                                 request_id);
}

void PluginProcessHost::OnSyncLoad(
    int request_id,
    const ViewHostMsg_Resource_Request& request,
    IPC::Message* sync_result) {
  int render_process_host_id = -1;
  URLRequestContext* context = CPBrowsingContextManager::Instance()->
        ToURLRequestContext(request.request_context);
  // TODO(mpcomplete): remove fallback case when Gears support is prevalent.
  if (!context)
    context = Profile::GetDefaultRequestContext();

  resource_dispatcher_host_->BeginRequest(this,
                                          process_.handle(),
                                          render_process_host_id,
                                          MSG_ROUTING_CONTROL,
                                          request_id,
                                          request,
                                          context,
                                          sync_result);
}

void PluginProcessHost::OnGetCookies(uint32 request_context,
                                     const GURL& url,
                                     std::string* cookies) {
  URLRequestContext* context = CPBrowsingContextManager::Instance()->
        ToURLRequestContext(request_context);
  // TODO(mpcomplete): remove fallback case when Gears support is prevalent.
  if (!context)
    context = Profile::GetDefaultRequestContext();

  // Note: We don't have a policy_url check because plugins bypass the
  // third-party cookie blocking.
  *cookies = context->cookie_store()->GetCookies(url);
}

void PluginProcessHost::ReplyToRenderer(
    ResourceMessageFilter* renderer_message_filter,
    const std::wstring& channel, const std::wstring& plugin_path,
    IPC::Message* reply_msg) {
  ViewHostMsg_OpenChannelToPlugin::WriteReplyParams(reply_msg, channel,
                                                    plugin_path);
  renderer_message_filter->Send(reply_msg);
}

void PluginProcessHost::RequestPluginChannel(
    ResourceMessageFilter* renderer_message_filter,
    const std::string& mime_type, IPC::Message* reply_msg) {
  // We can't send any sync messages from the browser because it might lead to
  // a hang.  However this async messages must be answered right away by the
  // plugin process (i.e. unblocks a Send() call like a sync message) otherwise
  // a deadlock can occur if the plugin creation request from the renderer is
  // a result of a sync message by the plugin process.

  // The plugin process expects to receive a handle to the renderer requesting
  // the channel. The handle has to be valid in the plugin process.
  HANDLE renderer_handle = NULL;
  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                renderer_message_filter->renderer_handle(),
                                process(), &renderer_handle, 0, FALSE,
                                DUPLICATE_SAME_ACCESS);
  DCHECK(result);

  PluginProcessMsg_CreateChannel* msg =
      new PluginProcessMsg_CreateChannel(
          renderer_message_filter->render_process_host_id(), renderer_handle);
  msg->set_unblock(true);
  if (Send(msg)) {
    sent_requests_.push_back(ChannelRequest(renderer_message_filter, mime_type,
                             reply_msg));
  } else {
    ReplyToRenderer(renderer_message_filter, std::wstring(), std::wstring(),
                    reply_msg);
  }
}

void PluginProcessHost::OnChannelCreated(int process_id,
                                         const std::wstring& channel_name) {
  for (size_t i = 0; i < sent_requests_.size(); ++i) {
    if (sent_requests_[i].renderer_message_filter_->render_process_host_id()
        == process_id) {
      ReplyToRenderer(sent_requests_[i].renderer_message_filter_.get(),
                      channel_name,
                      dll_path(),
                      sent_requests_[i].reply_msg);

      sent_requests_.erase(sent_requests_.begin() + i);
      return;
    }
  }

  NOTREACHED();
}

void PluginProcessHost::OnDownloadUrl(const std::string& url,
                                      int source_pid, HWND caller_window) {
  PluginDownloadUrlHelper* download_url_helper =
      new PluginDownloadUrlHelper(url, source_pid, caller_window);
  download_url_helper->InitiateDownload();
}

void PluginProcessHost::OnGetPluginFinderUrl(std::string* plugin_finder_url) {
  if (!plugin_finder_url) {
    NOTREACHED();
    return;
  }

  // TODO(iyengar)  Add the plumbing to retrieve the default
  // plugin finder URL.
  *plugin_finder_url = kDefaultPluginFinderURL;
}

void PluginProcessHost::OnPluginShutdownRequest() {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  // If we have pending channel open requests from the renderers, then
  // refuse the shutdown request from the plugin process.
  bool ok_to_shutdown = sent_requests_.empty();

  if (ok_to_shutdown)
    plugin_service_->OnPluginProcessIsShuttingDown(this);

  Send(new PluginProcessMsg_ShutdownResponse(ok_to_shutdown));
}

void PluginProcessHost::OnPluginMessage(
    const std::vector<uint8>& data) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(dll_path_);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
}

void PluginProcessHost::Shutdown() {

  Send(new PluginProcessMsg_BrowserShutdown);
}

