// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_utils.h"

#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/page_navigator.h"
#include "chrome/browser/tab_contents.h"
#include "chrome/common/drag_drop_types.h"
#include "chrome/common/l10n_util.h"
#include "chrome/common/os_exchange_data.h"
#include "chrome/views/event.h"

#include "chromium_strings.h"
#include "generated_resources.h"

namespace {

// A PageNavigator implementation that creates a new Browser. This is used when
// opening a url and there is no Browser open. The Browser is created the first
// time the PageNavigator method is invoked.
class NewBrowserPageNavigator : public PageNavigator {
 public:
  explicit NewBrowserPageNavigator(Profile* profile)
      : profile_(profile),
        browser_(NULL) {}

  virtual ~NewBrowserPageNavigator() {
    if (browser_)
      browser_->Show();
  }

  Browser* browser() const { return browser_; }

  virtual void OpenURL(const GURL& url,
                       const GURL& referrer,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition) {
    if (!browser_) {
      Profile* profile = (disposition == OFF_THE_RECORD) ?
          profile_->GetOffTheRecordProfile() : profile_;
      browser_ = new Browser(gfx::Rect(), SW_SHOW, profile,
                             BrowserType::TABBED_BROWSER, std::wstring());
      // Always open the first tab in the foreground.
      disposition = NEW_FOREGROUND_TAB;
    }
    browser_->OpenURLFromTab(NULL, url, referrer, NEW_FOREGROUND_TAB, transition);
  }

 private:
  Profile* profile_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NewBrowserPageNavigator);
};

void CloneDragDataImpl(BookmarkModel* model,
                       const BookmarkDragData::Element& element,
                       BookmarkNode* parent,
                       int index_to_add_at) {
  if (element.is_url) {
    model->AddURL(parent, index_to_add_at, element.title, element.url);
  } else {
    BookmarkNode* new_folder = model->AddGroup(parent, index_to_add_at,
                                               element.title);
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneDragDataImpl(model, element.children[i], new_folder, i);
  }
}

// Returns the number of descendants of node that are of type url.
int DescendantURLCount(BookmarkNode* node) {
  int result = 0;
  for (int i = 0; i < node->GetChildCount(); ++i) {
    BookmarkNode* child = node->GetChild(i);
    if (child->is_url())
      result++;
    else
      result += DescendantURLCount(child);
  }
  return result;
}

// Implementation of OpenAll. Opens all nodes of type URL and recurses for
// groups. |navigator| is the PageNavigator used to open URLs. After the first
// url is opened |opened_url| is set to true and |navigator| is set to the
// PageNavigator of the last active tab. This is done to handle a window
// disposition of new window, in which case we want subsequent tabs to open in
// that window.
void OpenAllImpl(BookmarkNode* node,
                 WindowOpenDisposition initial_disposition,
                 PageNavigator** navigator,
                 bool* opened_url) {
  if (node->is_url()) {
    WindowOpenDisposition disposition;
    if (*opened_url)
      disposition = NEW_BACKGROUND_TAB;
    else
      disposition = initial_disposition;
    (*navigator)->OpenURL(node->GetURL(), GURL(), disposition,
                          PageTransition::AUTO_BOOKMARK);
    if (!*opened_url) {
      *opened_url = true;
      // We opened the first URL which may have opened a new window or clobbered
      // the current page, reset the navigator just to be sure.
      Browser* new_browser = BrowserList::GetLastActive();
      if (new_browser) {
        TabContents* current_tab = new_browser->GetSelectedTabContents();
        DCHECK(new_browser && current_tab);
        if (new_browser && current_tab)
          *navigator = current_tab;
      }  // else, new_browser == NULL, which happens during testing.
    }
  } else {
    // Group, recurse through children.
    for (int i = 0; i < node->GetChildCount(); ++i) {
      OpenAllImpl(node->GetChild(i), initial_disposition, navigator,
                  opened_url);
    }
  }
}

bool ShouldOpenAll(HWND parent, const std::vector<BookmarkNode*>& nodes) {
  int descendant_count = 0;
  for (size_t i = 0; i < nodes.size(); ++i)
    descendant_count += DescendantURLCount(nodes[i]);
  if (descendant_count < bookmark_utils::num_urls_before_prompting)
    return true;

  std::wstring message =
      l10n_util::GetStringF(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                            IntToWString(descendant_count));
  return MessageBox(parent, message.c_str(),
                    l10n_util::GetString(IDS_PRODUCT_NAME).c_str(),
                    MB_YESNO | MB_ICONWARNING | MB_TOPMOST) == IDYES;
}

}  // namespace

namespace bookmark_utils {

int num_urls_before_prompting = 15;

int PreferredDropOperation(int source_operations, int operations) {
  int common_ops = (source_operations & operations);
  if (!common_ops)
    return 0;
  if (DragDropTypes::DRAG_COPY & common_ops)
    return DragDropTypes::DRAG_COPY;
  if (DragDropTypes::DRAG_LINK & common_ops)
    return DragDropTypes::DRAG_LINK;
  if (DragDropTypes::DRAG_MOVE & common_ops)
    return DragDropTypes::DRAG_MOVE;
  return DragDropTypes::DRAG_NONE;
}

bool IsValidDropLocation(Profile* profile,
                         const BookmarkDragData& data,
                         BookmarkNode* drop_parent,
                         int index) {
  if (!drop_parent->is_folder()) {
    NOTREACHED();
    return false;
  }

  if (!data.is_valid())
    return false;

  if (data.IsFromProfile(profile)) {
    std::vector<BookmarkNode*> nodes = data.GetNodes(profile);
    for (size_t i = 0; i < nodes.size(); ++i) {
      // Don't allow the drop if the user is attempting to drop on one of the
      // nodes being dragged.
      BookmarkNode* node = nodes[i];
      int node_index = (drop_parent == node->GetParent()) ?
          drop_parent->IndexOfChild(nodes[i]) : -1;
      if (node_index != -1 && (index == node_index || index == node_index + 1))
        return false;

      // drop_parent can't accept a child that is an ancestor.
      if (drop_parent->HasAncestor(node))
        return false;
    }
    return true;
  }
  // From the same profile, always accept.
  return true;
}

void CloneDragData(BookmarkModel* model,
                   const std::vector<BookmarkDragData::Element>& elements,
                   BookmarkNode* parent,
                   int index_to_add_at) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < elements.size(); ++i)
    CloneDragDataImpl(model, elements[i], parent, index_to_add_at + i);
}

void OpenAll(HWND parent,
             Profile* profile,
             PageNavigator* navigator,
             const std::vector<BookmarkNode*>& nodes,
             WindowOpenDisposition initial_disposition) {
  if (!ShouldOpenAll(parent, nodes))
    return;

  NewBrowserPageNavigator navigator_impl(profile);
  if (!navigator) {
    Browser* browser = 
        BrowserList::FindBrowserWithType(profile, BrowserType::TABBED_BROWSER);
    if (!browser || !browser->GetSelectedTabContents())
      navigator = &navigator_impl;
    else
      navigator = browser->GetSelectedTabContents();
  }

  bool opened_url = false;
  for (size_t i = 0; i < nodes.size(); ++i)
    OpenAllImpl(nodes[i], initial_disposition, &navigator, &opened_url);
}

void OpenAll(HWND parent,
             Profile* profile,
             PageNavigator* navigator,
             BookmarkNode* node,
             WindowOpenDisposition initial_disposition) {
  std::vector<BookmarkNode*> nodes;
  nodes.push_back(node);
  OpenAll(parent, profile, navigator, nodes, initial_disposition);
}

void CopyToClipboard(BookmarkModel* model,
                     const std::vector<BookmarkNode*>& nodes,
                     bool remove_nodes) {
  if (nodes.empty())
    return;

  OSExchangeData* data = new OSExchangeData();
  BookmarkDragData(nodes).Write(NULL, data);
  OleSetClipboard(data);
  // OLE takes ownership of OSExchangeData.

  if (remove_nodes) {
    for (size_t i = 0; i < nodes.size(); ++i) {
      model->Remove(nodes[i]->GetParent(),
                    nodes[i]->GetParent()->IndexOfChild(nodes[i]));
    }
  }
}

void PasteFromClipboard(BookmarkModel* model,
                        BookmarkNode* parent,
                        int index) {
  if (!parent)
    return;

  IDataObject* data;
  if (OleGetClipboard(&data) != S_OK)
    return;

  OSExchangeData data_wrapper(data);
  BookmarkDragData bookmark_data;
  if (!bookmark_data.Read(data_wrapper))
    return;

  if (index == -1)
    index = parent->GetChildCount();
  bookmark_utils::CloneDragData(model, bookmark_data.elements, parent, index);
}

bool CanPasteFromClipboard(BookmarkNode* node) {
  if (!node)
    return false;

  IDataObject* data;
  if (OleGetClipboard(&data) != S_OK)
    return false;

  OSExchangeData data_wrapper(data);
  BookmarkDragData bookmark_data;
  return bookmark_data.Read(data_wrapper);
}

}  // namespace bookmark_utils
