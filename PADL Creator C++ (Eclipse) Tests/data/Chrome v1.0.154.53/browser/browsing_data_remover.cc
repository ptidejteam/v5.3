// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_remover.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_service.h"
#include "chrome/browser/tab_restore_service.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/user_metrics.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/notification_service.h"
#include "net/base/cookie_monster.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "webkit/glue/password_form.h"

// Done so that we can use invokeLater on BrowsingDataRemovers and not have
// BrowsingDataRemover implement RefCounted.
void RunnableMethodTraits<BrowsingDataRemover>::RetainCallee(
    BrowsingDataRemover* remover) {
}
void RunnableMethodTraits<BrowsingDataRemover>::ReleaseCallee(
    BrowsingDataRemover* remover) {
}

BrowsingDataRemover::BrowsingDataRemover(Profile* profile, Time delete_begin,
                                         Time delete_end)
    : profile_(profile),
      delete_begin_(delete_begin),
      delete_end_(delete_end),
      removing_(false),
      waiting_for_keywords_(false),
      waiting_for_clear_history_(false),
      waiting_for_clear_cache_(false) {
  DCHECK(profile);
}

BrowsingDataRemover::~BrowsingDataRemover() {
  DCHECK(all_done());
}

void BrowsingDataRemover::Remove(int remove_mask) {
  DCHECK(!removing_);
  removing_ = true;

  if (remove_mask & REMOVE_HISTORY) {
    HistoryService* history_service =
        profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (history_service) {
      UserMetrics::RecordAction(L"ClearBrowsingData_History", profile_);
      waiting_for_clear_history_ = true;
      history_service->ExpireHistoryBetween(delete_begin_, delete_end_,
          &request_consumer_,
          NewCallback(this, &BrowsingDataRemover::OnHistoryDeletionDone));
    }

    // As part of history deletion we also delete the auto-generated keywords.
    TemplateURLModel* keywords_model = profile_->GetTemplateURLModel();
    if (keywords_model && !keywords_model->loaded()) {
      waiting_for_keywords_ = true;
      NotificationService::current()->AddObserver(
          this, TEMPLATE_URL_MODEL_LOADED,
          Source<TemplateURLModel>(keywords_model));
      keywords_model->Load();
    } else if (keywords_model) {
      keywords_model->RemoveAutoGeneratedBetween(delete_begin_, delete_end_);
    }

    // We also delete the list of recently closed tabs. Since these expire,
    // they can't be more than a day old, so we can simply clear them all.
    TabRestoreService* tab_service = profile_->GetTabRestoreService();
    if (tab_service)
      tab_service->ClearHistoricalTabs();

    // We also delete the last session when we delete the history.
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->DeleteSession(false);   // Last session.
  }

  if (remove_mask & REMOVE_DOWNLOADS) {
    UserMetrics::RecordAction(L"ClearBrowsingData_Downloads", profile_);
    DownloadManager* download_manager = profile_->GetDownloadManager();
    download_manager->RemoveDownloadsBetween(delete_begin_, delete_end_);
  }

  if (remove_mask & REMOVE_COOKIES) {
    UserMetrics::RecordAction(L"ClearBrowsingData_Cookies", profile_);
    net::CookieMonster* cookie_monster =
        profile_->GetRequestContext()->cookie_store();
    cookie_monster->DeleteAllCreatedBetween(delete_begin_, delete_end_, true);
  }

  if (remove_mask & REMOVE_PASSWORDS) {
    UserMetrics::RecordAction(L"ClearBrowsingData_Passwords", profile_);
    WebDataService* web_data_service =
        profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);

    web_data_service->RemoveLoginsCreatedBetween(delete_begin_, delete_end_);
  }

  if (remove_mask & REMOVE_CACHE) {
    // Invoke ClearBrowsingDataView::ClearCache on the IO thread.
    base::Thread* thread = g_browser_process->io_thread();
    if (thread) {
      waiting_for_clear_cache_ = true;
      UserMetrics::RecordAction(L"ClearBrowsingData_Cache", profile_);
      thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
          this,
          &BrowsingDataRemover::ClearCacheOnIOThread,
          delete_begin_,
          delete_end_,
          MessageLoop::current()));
    }
  }

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void BrowsingDataRemover::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void BrowsingDataRemover::OnHistoryDeletionDone() {
  waiting_for_clear_history_ = false;
  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  // TODO(brettw) bug 1139736: This should also observe session
  // clearing (what about other things such as passwords, etc.?) and wait for
  // them to complete before continuing.
  DCHECK(type == TEMPLATE_URL_MODEL_LOADED);
  TemplateURLModel* model = Source<TemplateURLModel>(source).ptr();
  if (model->profile() == profile_->GetOriginalProfile()) {
    NotificationService::current()->RemoveObserver(
        this, TEMPLATE_URL_MODEL_LOADED,
        Source<TemplateURLModel>(model));

    model->RemoveAutoGeneratedBetween(delete_begin_, delete_end_);

    waiting_for_keywords_ = false;

    NotifyAndDeleteIfDone();
  }
}

void BrowsingDataRemover::NotifyAndDeleteIfDone() {
  // TODO(brettw) bug 1139736: see TODO in Observe() above.
  if (!all_done())
    return;

  FOR_EACH_OBSERVER(Observer, observer_list_, OnBrowsingDataRemoverDone());

  // History requests aren't happy if you delete yourself from the callback.
  // As such, we do a delete later.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void BrowsingDataRemover::ClearedCache() {
  waiting_for_clear_cache_ = false;

  NotifyAndDeleteIfDone();
}

void BrowsingDataRemover::ClearCacheOnIOThread(Time delete_begin,
                                               Time delete_end,
                                               MessageLoop* ui_loop) {
  // This function should be called on the IO thread.
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Get a pointer to the cache.
  net::HttpTransactionFactory* factory =
      profile_->GetRequestContext()->http_transaction_factory();
  disk_cache::Backend* cache = factory->GetCache()->disk_cache();

  // The cache can be empty, for example on startup, in which case |cache| will
  // be null and we don't need to do anything.
  if (cache) {
    if (delete_begin.is_null())
      cache->DoomAllEntries();
    else
      cache->DoomEntriesBetween(delete_begin, delete_end);
  }

  // Notify the UI thread that we are done.
  ui_loop->PostTask(FROM_HERE, NewRunnableMethod(
      this, &BrowsingDataRemover::ClearedCache));
}

