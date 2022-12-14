// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/template_url_fetcher.h"

#include "chrome/browser/profile.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/browser/template_url_parser.h"
#include "chrome/browser/views/edit_keyword_controller.h"

// RequestDelegate ------------------------------------------------------------

void TemplateURLFetcher::RequestDelegate::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // Make sure we can still replace the keyword.
  if (response_code != 200) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  scoped_ptr<TemplateURL> template_url(new TemplateURL());
  if (TemplateURLParser::Parse(
          reinterpret_cast<const unsigned char*>(data.c_str()),
                                                 data.length(),
                                                 NULL,
                                                 template_url.get()) &&
      template_url->url() && template_url->url()->SupportsReplacement()) {
    TemplateURLModel* model = fetcher_->profile()->GetTemplateURLModel();
    const TemplateURL* existing_url;
    if (!model || !model->loaded() ||
        !model->CanReplaceKeyword(keyword_, template_url->url()->url(),
                                  &existing_url)) {
      // TODO(pamg): If we're coming from JS (not autodetected) and this URL
      // already exists in the model, consider bringing up the
      // EditKeywordController to edit it.  This would be helpful feedback in
      // the case of clicking a button twice, and annoying in the case of a
      // page that calls AddSearchProvider() in JS without a user action.
      fetcher_->RequestCompleted(this);
      // WARNING: RequestCompleted deletes us.
      return;
    }

    if (existing_url)
      model->Remove(existing_url);

    // The short name is what is shown to the user. We reset it to make sure
    // we don't display random text from the web.
    template_url->set_short_name(keyword_);
    template_url->set_keyword(keyword_);
    template_url->set_originating_url(osdd_url_);

    // The page may have specified a URL to use for favicons, if not, set it.
    if (!template_url->GetFavIconURL().is_valid())
      template_url->SetFavIconURL(favicon_url_);

    if (autodetected_) {
      // Mark the keyword as replaceable so it can be removed if necessary.
      template_url->set_safe_for_autoreplace(true);
      model->Add(template_url.release());
    } else {
      // Confirm addition and allow user to edit default choices. It's ironic
      // that only *non*-autodetected additions get confirmed, but the user
      // expects feedback that his action did something.
      // The edit controller will take care of adding the URL to the model,
      // which takes ownership, or of deleting it if the add is cancelled.
      EditKeywordController* controller =
          new EditKeywordController(parent_window_,
                                    template_url.release(),
                                    NULL,  // no KeywordEditorView
                                    fetcher_->profile());
      controller->Show();
    }
  }
  fetcher_->RequestCompleted(this);
  // WARNING: RequestCompleted deletes us.
}

// TemplateURLFetcher ---------------------------------------------------------

TemplateURLFetcher::TemplateURLFetcher(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

void TemplateURLFetcher::ScheduleDownload(const std::wstring& keyword,
                                          const GURL& osdd_url,
                                          const GURL& favicon_url,
                                          const HWND parent_window,
                                          bool autodetected) {
  DCHECK(!keyword.empty() && osdd_url.is_valid());
  // Make sure we aren't already downloading this request.
  for (std::vector<RequestDelegate*>::iterator i = requests_->begin();
       i != requests_->end(); ++i) {
    if ((*i)->url() == osdd_url || (*i)->keyword() == keyword)
      return;
  }

  requests_->push_back(
      new RequestDelegate(this, keyword, osdd_url, favicon_url, parent_window,
                          autodetected));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  DCHECK(find(requests_->begin(), requests_->end(), request) !=
         requests_->end());
  requests_->erase(find(requests_->begin(), requests_->end(), request));
  delete request;
}

