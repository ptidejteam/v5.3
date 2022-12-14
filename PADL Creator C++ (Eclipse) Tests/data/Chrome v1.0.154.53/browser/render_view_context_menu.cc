// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/render_view_context_menu.h"

#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/template_url_model.h"
#include "chrome/common/l10n_util.h"
#include "webkit/glue/context_node_types.h"

#include "generated_resources.h"

RenderViewContextMenu::RenderViewContextMenu(
    Menu::Delegate* delegate,
    HWND owner,
    ContextNode::Type type,
    const std::wstring& misspelled_word,
    const std::vector<std::wstring>& misspelled_word_suggestions,
    Profile* profile)
    : Menu(delegate, Menu::TOPLEFT, owner),
      misspelled_word_(misspelled_word),
      misspelled_word_suggestions_(misspelled_word_suggestions),
      profile_(profile) {
  InitMenu(type);
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

void RenderViewContextMenu::InitMenu(ContextNode::Type type) {
  switch (type) {
   case ContextNode::PAGE:
    AppendPageItems();
    break;
   case ContextNode::FRAME:
    AppendFrameItems();
    break;
   case ContextNode::LINK:
    AppendLinkItems();
    break;
   case ContextNode::IMAGE:
    AppendImageItems();
    break;
   case ContextNode::IMAGE_LINK:
    AppendLinkItems();
    AppendSeparator();
    AppendImageItems();
    break;
   case ContextNode::SELECTION:
    AppendSelectionItems();
    break;
   case ContextNode::EDITABLE:
    AppendEditableItems();
    break;
   default:
    NOTREACHED() << "Unknown ContextNode::Type";
  }
  AppendSeparator();
  AppendDeveloperItems();
}

void RenderViewContextMenu::AppendDeveloperItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendLinkItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVELINKAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPYLINKLOCATION);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendImageItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPYIMAGE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

void RenderViewContextMenu::AppendPageItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_RELOAD);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_PRINT);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWPAGEINFO);
}

void RenderViewContextMenu::AppendFrameItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_BACK);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_FORWARD);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWTAB);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMENEWWINDOW);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_OPENFRAMEOFFTHERECORD);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SAVEFRAMEAS);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_PRINTFRAME);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_VIEWFRAMEINFO);
}

void RenderViewContextMenu::AppendSelectionItems() {
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPY);
  DCHECK(profile_);
  if (profile_->GetTemplateURLModel()->GetDefaultSearchProvider() != NULL)
    AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SEARCHWEBFOR);
}

void RenderViewContextMenu::AppendEditableItems() {
  // Append Dictionary spell check suggestions.
  for (size_t i = 0; i < misspelled_word_suggestions_.size() &&
       IDC_USESPELLCHECKSUGGESTION_0 + i <= IDC_USESPELLCHECKSUGGESTION_LAST;
       i ++) {
    AppendMenuItemWithLabel(IDC_USESPELLCHECKSUGGESTION_0 + static_cast<int>(i),
                            misspelled_word_suggestions_[i]);
  }
  if (misspelled_word_suggestions_.size() > 0)
    AppendSeparator();
  
  // If word is misspelled, give option for "Add to dictionary"
  if (!misspelled_word_.empty()) {
    if (misspelled_word_suggestions_.size() == 0) {
      AppendMenuItemWithLabel(0, 
          l10n_util::GetString(IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    }
    AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY);
    AppendSeparator();
  }

  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_UNDO);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_REDO);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_CUT);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_COPY);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_PASTE);
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_DELETE);
  AppendSeparator();
  AppendDelegateMenuItem(IDS_CONTENT_CONTEXT_SELECTALL);
}

