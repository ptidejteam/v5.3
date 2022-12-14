// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_codec.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/test/testing_profile.h"
#include "chrome/views/tree_node_model.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkModelTest : public testing::Test, public BookmarkModelObserver {
 public:
  struct ObserverDetails {
    ObserverDetails() {
      Set(NULL, NULL, -1, -1);
    }

    void Set(BookmarkNode* node1,
             BookmarkNode* node2,
             int index1,
             int index2) {
      this->node1 = node1;
      this->node2 = node2;
      this->index1 = index1;
      this->index2 = index2;
    }

    void AssertEquals(BookmarkNode* node1,
                      BookmarkNode* node2,
                      int index1,
                      int index2) {
      ASSERT_TRUE(this->node1 == node1);
      ASSERT_TRUE(this->node2 == node2);
      ASSERT_EQ(index1, this->index1);
      ASSERT_EQ(index2, this->index2);
    }

    BookmarkNode* node1;
    BookmarkNode* node2;
    int index1;
    int index2;
  };

  BookmarkModelTest() : model(NULL) {
    model.AddObserver(this);
    ClearCounts();
  }


  void Loaded(BookmarkModel* model) {
    // We never load from the db, so that this should never get invoked.
    NOTREACHED();
  }

  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {
    moved_count++;
    observer_details.Set(old_parent, new_parent, old_index, new_index);
  }

  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {
    added_count++;
    observer_details.Set(parent, NULL, index, -1);
  }

  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index) {
    removed_count++;
    observer_details.Set(parent, NULL, index, -1);
  }

  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) {
    changed_count++;
    observer_details.Set(node, NULL, -1, -1);
  }

  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {
    // We never attempt to load favicons, so that this method never
    // gets invoked.
  }

  void ClearCounts() {
    moved_count = added_count = removed_count = changed_count = 0;
  }

  void AssertObserverCount(int added_count,
                           int moved_count,
                           int removed_count,
                           int changed_count) {
    ASSERT_EQ(added_count, this->added_count);
    ASSERT_EQ(moved_count, this->moved_count);
    ASSERT_EQ(removed_count, this->removed_count);
    ASSERT_EQ(changed_count, this->changed_count);
  }

  void AssertNodesEqual(BookmarkNode* expected, BookmarkNode* actual) {
    ASSERT_TRUE(expected);
    ASSERT_TRUE(actual);
    EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
    EXPECT_EQ(expected->GetType(), actual->GetType());
    EXPECT_TRUE(expected->date_added() == actual->date_added());
    if (expected->GetType() == history::StarredEntry::URL) {
      EXPECT_EQ(expected->GetURL(), actual->GetURL());
    } else {
      EXPECT_TRUE(expected->date_group_modified() ==
                  actual->date_group_modified());
      ASSERT_EQ(expected->GetChildCount(), actual->GetChildCount());
      for (int i = 0; i < expected->GetChildCount(); ++i)
        AssertNodesEqual(expected->GetChild(i), actual->GetChild(i));
    }
  }

  void AssertModelsEqual(BookmarkModel* expected,
                         BookmarkModel* actual) {
    AssertNodesEqual(expected->GetBookmarkBarNode(),
                     actual->GetBookmarkBarNode());
    AssertNodesEqual(expected->other_node(),
                     actual->other_node());
  }

  BookmarkModel model;

  int moved_count;

  int added_count;

  int removed_count;

  int changed_count;

  ObserverDetails observer_details;
};

TEST_F(BookmarkModelTest, InitialState) {
  BookmarkNode* bb_node = model.GetBookmarkBarNode();
  ASSERT_TRUE(bb_node != NULL);
  EXPECT_EQ(0, bb_node->GetChildCount());
  EXPECT_EQ(history::StarredEntry::BOOKMARK_BAR, bb_node->GetType());

  BookmarkNode* other_node = model.other_node();
  ASSERT_TRUE(other_node != NULL);
  EXPECT_EQ(0, other_node->GetChildCount());
  EXPECT_EQ(history::StarredEntry::OTHER, other_node->GetType());

  EXPECT_TRUE(bb_node->id() != other_node->id());
}

TEST_F(BookmarkModelTest, AddURL) {
  BookmarkNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");

  BookmarkNode* new_node = model.AddURL(root, 0, title, url);
  AssertObserverCount(1, 0, 0, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->GetChildCount());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_TRUE(url == new_node->GetURL());
  ASSERT_EQ(history::StarredEntry::URL, new_node->GetType());
  ASSERT_TRUE(new_node == model.GetMostRecentlyAddedNodeForURL(url));

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model.other_node()->id());
}

TEST_F(BookmarkModelTest, AddGroup) {
  BookmarkNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");

  BookmarkNode* new_node = model.AddGroup(root, 0, title);
  AssertObserverCount(1, 0, 0, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  ASSERT_EQ(1, root->GetChildCount());
  ASSERT_EQ(title, new_node->GetTitle());
  ASSERT_EQ(history::StarredEntry::USER_GROUP, new_node->GetType());

  EXPECT_TRUE(new_node->id() != root->id() &&
              new_node->id() != model.other_node()->id());

  // Add another group, just to make sure group_ids are incremented correctly.
  ClearCounts();
  BookmarkNode* new_node2 = model.AddGroup(root, 0, title);
  AssertObserverCount(1, 0, 0, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);
}

TEST_F(BookmarkModelTest, RemoveURL) {
  BookmarkNode* root = model.GetBookmarkBarNode();
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkNode* new_node = model.AddURL(root, 0, title, url);
  ClearCounts();

  model.Remove(root, 0);
  ASSERT_EQ(0, root->GetChildCount());
  AssertObserverCount(0, 0, 1, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model.GetMostRecentlyAddedNodeForURL(url) == NULL);
}

TEST_F(BookmarkModelTest, RemoveGroup) {
  BookmarkNode* root = model.GetBookmarkBarNode();
  BookmarkNode* group = model.AddGroup(root, 0, L"foo");

  ClearCounts();

  // Add a URL as a child.
  const std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkNode* new_node = model.AddURL(group, 0, title, url);

  ClearCounts();

  // Now remove the group.
  model.Remove(root, 0);
  ASSERT_EQ(0, root->GetChildCount());
  AssertObserverCount(0, 0, 1, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);

  // Make sure there is no mapping for the URL.
  ASSERT_TRUE(model.GetMostRecentlyAddedNodeForURL(url) == NULL);
}

TEST_F(BookmarkModelTest, SetTitle) {
  BookmarkNode* root = model.GetBookmarkBarNode();
  std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkNode* node = model.AddURL(root, 0, title, url);

  ClearCounts();

  title = L"foo2";
  model.SetTitle(node, title);
  AssertObserverCount(0, 0, 0, 1);
  observer_details.AssertEquals(node, NULL, -1, -1);
  EXPECT_EQ(title, node->GetTitle());
}

TEST_F(BookmarkModelTest, Move) {
  BookmarkNode* root = model.GetBookmarkBarNode();
  std::wstring title(L"foo");
  const GURL url("http://foo.com");
  BookmarkNode* node = model.AddURL(root, 0, title, url);
  BookmarkNode* group1 = model.AddGroup(root, 0, L"foo");
  ClearCounts();

  model.Move(node, group1, 0);

  AssertObserverCount(0, 1, 0, 0);
  observer_details.AssertEquals(root, group1, 1, 0);
  EXPECT_TRUE(group1 == node->GetParent());
  EXPECT_EQ(1, root->GetChildCount());
  EXPECT_EQ(group1, root->GetChild(0));
  EXPECT_EQ(1, group1->GetChildCount());
  EXPECT_EQ(node, group1->GetChild(0));

  // And remove the group.
  ClearCounts();
  model.Remove(root, 0);
  AssertObserverCount(0, 0, 1, 0);
  observer_details.AssertEquals(root, NULL, 0, -1);
  EXPECT_TRUE(model.GetMostRecentlyAddedNodeForURL(url) == NULL);
  EXPECT_EQ(0, root->GetChildCount());
}

// Tests that adding a URL to a folder updates the last modified time.
TEST_F(BookmarkModelTest, ParentForNewNodes) {
  ASSERT_EQ(model.GetBookmarkBarNode(), model.GetParentForNewNodes());

  const std::wstring title(L"foo");
  const GURL url("http://foo.com");

  BookmarkNode* new_node = model.AddURL(model.other_node(), 0, title, url);

  ASSERT_EQ(model.other_node(), model.GetParentForNewNodes());
}

// Make sure recently modified stays in sync when adding a URL.
TEST_F(BookmarkModelTest, MostRecentlyModifiedGroups) {
  // Add a group.
  BookmarkNode* group = model.AddGroup(model.other_node(), 0, L"foo");
  // Add a URL to it.
  model.AddURL(group, 0, L"blah", GURL("http://foo.com"));

  // Make sure group is in the most recently modified.
  std::vector<BookmarkNode*> most_recent_groups =
      model.GetMostRecentlyModifiedGroups(1);
  ASSERT_EQ(1, most_recent_groups.size());
  ASSERT_EQ(group, most_recent_groups[0]);

  // Nuke the group and do another fetch, making sure group isn't in the
  // returned list.
  model.Remove(group->GetParent(), 0);
  most_recent_groups = model.GetMostRecentlyModifiedGroups(1);
  ASSERT_EQ(1, most_recent_groups.size());
  ASSERT_TRUE(most_recent_groups[0] != group);
}

// Make sure MostRecentlyAddedEntries stays in sync.
TEST_F(BookmarkModelTest, MostRecentlyAddedEntries) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2 > n3 > n4.
  Time base_time = Time::Now();
  BookmarkNode* n1 = model.AddURL(
      model.GetBookmarkBarNode(), 0, L"blah", GURL("http://foo.com/0"));
  BookmarkNode* n2 = model.AddURL(
      model.GetBookmarkBarNode(), 1, L"blah", GURL("http://foo.com/1"));
  BookmarkNode* n3 = model.AddURL(
      model.GetBookmarkBarNode(), 2, L"blah", GURL("http://foo.com/2"));
  BookmarkNode* n4 = model.AddURL(
      model.GetBookmarkBarNode(), 3, L"blah", GURL("http://foo.com/3"));
  n1->date_added_ = base_time + TimeDelta::FromDays(4);
  n2->date_added_ = base_time + TimeDelta::FromDays(3);
  n3->date_added_ = base_time + TimeDelta::FromDays(2);
  n4->date_added_ = base_time + TimeDelta::FromDays(1);

  // Make sure order is honored.
  std::vector<BookmarkNode*> recently_added;
  model.GetMostRecentlyAddedEntries(2, &recently_added);
  ASSERT_EQ(2, recently_added.size());
  ASSERT_TRUE(n1 == recently_added[0]);
  ASSERT_TRUE(n2 == recently_added[1]);

  // swap 1 and 2, then check again.
  recently_added.clear();
  std::swap(n1->date_added_, n2->date_added_);
  model.GetMostRecentlyAddedEntries(4, &recently_added);
  ASSERT_EQ(4, recently_added.size());
  ASSERT_TRUE(n2 == recently_added[0]);
  ASSERT_TRUE(n1 == recently_added[1]);
  ASSERT_TRUE(n3 == recently_added[2]);
  ASSERT_TRUE(n4 == recently_added[3]);
}

// Makes sure GetBookmarksMatchingText works.
TEST_F(BookmarkModelTest, GetBookmarksMatchingText) {
  // Add two urls with titles 'blah' and 'x' and one folder with the title
  // 'blah'.
  BookmarkNode* n1 = model.AddURL(
      model.GetBookmarkBarNode(), 0, L"blah", GURL("http://foo.com/0"));
  BookmarkNode* n2 = model.AddURL(
      model.GetBookmarkBarNode(), 1, L"x", GURL("http://foo.com/1"));
  model.AddGroup(model.GetBookmarkBarNode(), 2, L"blah");

  // Make sure we don't get back the folder.
  std::vector<BookmarkModel::TitleMatch> results;
  model.GetBookmarksMatchingText(L"blah", 2, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(n1, results[0].node);
  results.clear();

  model.GetBookmarksMatchingText(L"x", 2, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(n2, results[0].node);
}

// Makes sure GetMostRecentlyAddedNodeForURL stays in sync.
TEST_F(BookmarkModelTest, GetMostRecentlyAddedNodeForURL) {
  // Add a couple of nodes such that the following holds for the time of the
  // nodes: n1 > n2
  Time base_time = Time::Now();
  const GURL url("http://foo.com/0");
  BookmarkNode* n1 = model.AddURL(model.GetBookmarkBarNode(), 0, L"blah", url);
  BookmarkNode* n2 = model.AddURL(model.GetBookmarkBarNode(), 1, L"blah", url);
  n1->date_added_ = base_time + TimeDelta::FromDays(4);
  n2->date_added_ = base_time + TimeDelta::FromDays(3);

  // Make sure order is honored.
  ASSERT_EQ(n1, model.GetMostRecentlyAddedNodeForURL(url));

  // swap 1 and 2, then check again.
  std::swap(n1->date_added_, n2->date_added_);
  ASSERT_EQ(n2, model.GetMostRecentlyAddedNodeForURL(url));
}

// Makes sure GetBookmarks removes duplicates.
TEST_F(BookmarkModelTest, GetBookmarksWithDups) {
  const GURL url("http://foo.com/0");
  model.AddURL(model.GetBookmarkBarNode(), 0, L"blah", url);
  model.AddURL(model.GetBookmarkBarNode(), 1, L"blah", url);

  std::vector<GURL> urls;
  model.GetBookmarks(&urls);
  EXPECT_EQ(1, urls.size());
  ASSERT_TRUE(urls[0] == url);
}

namespace {

// NotificationObserver implementation used in verifying we've received the
// NOTIFY_URLS_STARRED method correctly.
class StarredListener : public NotificationObserver {
 public:
  StarredListener() : notification_count_(0), details_(false) {
    registrar_.Add(this, NOTIFY_URLS_STARRED, Source<Profile>(NULL));
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NOTIFY_URLS_STARRED) {
      notification_count_++;
      details_ = *(Details<history::URLsStarredDetails>(details).ptr());
    }
  }

  // Number of times NOTIFY_URLS_STARRED has been observed.
  int notification_count_;

  // Details from the last NOTIFY_URLS_STARRED.
  history::URLsStarredDetails details_;

 private:
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StarredListener);
};

}  // namespace

// Makes sure NOTIFY_URLS_STARRED is sent correctly.
TEST_F(BookmarkModelTest, NotifyURLsStarred) {
  StarredListener listener;
  const GURL url("http://foo.com/0");
  BookmarkNode* n1 = model.AddURL(model.GetBookmarkBarNode(), 0, L"blah", url);

  // Starred notification should be sent.
  EXPECT_EQ(1, listener.notification_count_);
  ASSERT_TRUE(listener.details_.starred);
  ASSERT_EQ(1, listener.details_.changed_urls.size());
  EXPECT_TRUE(url == *(listener.details_.changed_urls.begin()));
  listener.notification_count_ = 0;
  listener.details_.changed_urls.clear();

  // Add another bookmark for the same URL. This should not send any
  // notification.
  BookmarkNode* n2 = model.AddURL(model.GetBookmarkBarNode(), 1, L"blah", url);

  EXPECT_EQ(0, listener.notification_count_);

  // Remove n2.
  model.Remove(n2->GetParent(), 1);
  n2 = NULL;

  // Shouldn't have received any notification as n1 still exists with the same
  // URL.
  EXPECT_EQ(0, listener.notification_count_);

  EXPECT_TRUE(model.GetMostRecentlyAddedNodeForURL(url) == n1);

  // Remove n1.
  model.Remove(n1->GetParent(), 0);

  // Now we should get the notification.
  EXPECT_EQ(1, listener.notification_count_);
  ASSERT_FALSE(listener.details_.starred);
  ASSERT_EQ(1, listener.details_.changed_urls.size());
  EXPECT_TRUE(url == *(listener.details_.changed_urls.begin()));
}

namespace {

// See comment in PopulateNodeFromString.
typedef views::TreeNodeWithValue<history::StarredEntry::Type> TestNode;

// Does the work of PopulateNodeFromString. index gives the index of the current
// element in description to process.
static void PopulateNodeImpl(const std::vector<std::wstring>& description,
                             size_t* index,
                             TestNode* parent) {
  while (*index < description.size()) {
    const std::wstring& element = description[*index];
    (*index)++;
    if (element == L"[") {
      // Create a new group and recurse to add all the children.
      // Groups are given a unique named by way of an ever increasing integer
      // value. The groups need not have a name, but one is assigned to help
      // in debugging.
      static int next_group_id = 1;
      TestNode* new_node =
          new TestNode(IntToWString(next_group_id++),
                       history::StarredEntry::USER_GROUP);
      parent->Add(parent->GetChildCount(), new_node);
      PopulateNodeImpl(description, index, new_node);
    } else if (element == L"]") {
      // End the current group.
      return;
    } else {
      // Add a new URL.

      // All tokens must be space separated. If there is a [ or ] in the name it
      // likely means a space was forgotten.
      DCHECK(element.find('[') == std::string::npos);
      DCHECK(element.find(']') == std::string::npos);
      parent->Add(parent->GetChildCount(),
                  new TestNode(element, history::StarredEntry::URL));
    }
  }
}

// Creates and adds nodes to parent based on description. description consists
// of the following tokens (all space separated):
//   [ : creates a new USER_GROUP node. All elements following the [ until the
//       next balanced ] is encountered are added as children to the node.
//   ] : closes the last group created by [ so that any further nodes are added
//       to the current groups parent.
//   text: creates a new URL node.
// For example, "a [b] c" creates the following nodes:
//   a 1 c
//     |
//     b
// In words: a node of type URL with the title a, followed by a group node with
// the title 1 having the single child of type url with name b, followed by
// the url node with the title c.
//
// NOTE: each name must be unique, and groups are assigned a unique title by way
// of an increasing integer.
static void PopulateNodeFromString(const std::wstring& description,
                                   TestNode* parent) {
  std::vector<std::wstring> elements;
  size_t index = 0;
  SplitStringAlongWhitespace(description, &elements);
  PopulateNodeImpl(elements, &index, parent);
}

// Populates the BookmarkNode with the children of parent.
static void PopulateBookmarkNode(TestNode* parent,
                                 BookmarkModel* model,
                                 BookmarkNode* bb_node) {
  for (int i = 0; i < parent->GetChildCount(); ++i) {
    TestNode* child = parent->GetChild(i);
    if (child->value == history::StarredEntry::USER_GROUP) {
      BookmarkNode* new_bb_node =
          model->AddGroup(bb_node, i, child->GetTitle());
      PopulateBookmarkNode(child, model, new_bb_node);
    } else {
      model->AddURL(bb_node, i, child->GetTitle(),
                    GURL("http://" + WideToASCII(child->GetTitle())));
    }
  }
}

}  // namespace

// Test class that creates a BookmarkModel with a real history backend.
class BookmarkModelTestWithProfile : public testing::Test,
                                     public BookmarkModelObserver {
 public:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    profile_.reset(NULL);
  }

  // The profile.
  scoped_ptr<TestingProfile> profile_;

 protected:
  // Verifies the contents of the bookmark bar node match the contents of the
  // TestNode.
  void VerifyModelMatchesNode(TestNode* expected, BookmarkNode* actual) {
    ASSERT_EQ(expected->GetChildCount(), actual->GetChildCount());
    for (int i = 0; i < expected->GetChildCount(); ++i) {
      TestNode* expected_child = expected->GetChild(i);
      BookmarkNode* actual_child = actual->GetChild(i);
      ASSERT_EQ(expected_child->GetTitle(), actual_child->GetTitle());
      if (expected_child->value == history::StarredEntry::USER_GROUP) {
        ASSERT_TRUE(actual_child->GetType() ==
                    history::StarredEntry::USER_GROUP);
        // Recurse throught children.
        VerifyModelMatchesNode(expected_child, actual_child);
        if (HasFatalFailure())
          return;
      } else {
        // No need to check the URL, just the title is enough.
        ASSERT_TRUE(actual_child->GetType() ==
                    history::StarredEntry::URL);
      }
    }
  }

  void BlockTillBookmarkModelLoaded() {
    bb_model_ = profile_->GetBookmarkModel();
    if (!bb_model_->IsLoaded())
      BlockTillLoaded(bb_model_);
    else
      bb_model_->AddObserver(this);
  }

  // Destroys the current profile, creates a new one and creates the history
  // service.
  void RecreateProfile() {
    // Need to shutdown the old one before creating a new one.
    profile_.reset(NULL);
    profile_.reset(new TestingProfile());
    profile_->CreateHistoryService(true);
  }

  BookmarkModel* bb_model_;

 private:
  // Blocks until the BookmarkModel has finished loading.
  void BlockTillLoaded(BookmarkModel* model) {
    model->AddObserver(this);
    MessageLoop::current()->Run();
  }

  // BookmarkModelObserver methods.
  virtual void Loaded(BookmarkModel* model) {
    // Balances the call in BlockTillLoaded.
    MessageLoop::current()->Quit();
  }
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 BookmarkNode* old_parent,
                                 int old_index,
                                 BookmarkNode* new_parent,
                                 int new_index) {}
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 BookmarkNode* parent,
                                 int index) {}
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   BookmarkNode* parent,
                                   int index) {}
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   BookmarkNode* node) {}
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         BookmarkNode* node) {}

  MessageLoopForUI message_loop_;
};

// Creates a set of nodes in the bookmark bar model, then recreates the
// bookmark bar model which triggers loading from the db and checks the loaded
// structure to make sure it is what we first created.
TEST_F(BookmarkModelTestWithProfile, CreateAndRestore) {
  struct TestData {
    // Structure of the children of the bookmark bar model node.
    const std::wstring bbn_contents;
    // Structure of the children of the other node.
    const std::wstring other_contents;
  } data[] = {
    // See PopulateNodeFromString for a description of these strings.
    { L"", L"" },
    { L"a", L"b" },
    { L"a [ b ]", L"" },
    { L"", L"[ b ] a [ c [ d e [ f ] ] ]" },
    { L"a [ b ]", L"" },
    { L"a b c [ d e [ f ] ]", L"g h i [ j k [ l ] ]"},
  };
  for (int i = 0; i < arraysize(data); ++i) {
    // Recreate the profile. We need to reset with NULL first so that the last
    // HistoryService releases the locks on the files it creates and we can
    // delete them.
    profile_.reset(NULL);
    profile_.reset(new TestingProfile());
    profile_->CreateBookmarkModel(true);
    profile_->CreateHistoryService(true);
    BlockTillBookmarkModelLoaded();

    TestNode bbn;
    PopulateNodeFromString(data[i].bbn_contents, &bbn);
    PopulateBookmarkNode(&bbn, bb_model_, bb_model_->GetBookmarkBarNode());

    TestNode other;
    PopulateNodeFromString(data[i].other_contents, &other);
    PopulateBookmarkNode(&other, bb_model_, bb_model_->other_node());

    profile_->CreateBookmarkModel(false);
    BlockTillBookmarkModelLoaded();

    VerifyModelMatchesNode(&bbn, bb_model_->GetBookmarkBarNode());
    VerifyModelMatchesNode(&other, bb_model_->other_node());
  }
}

// Test class that creates a BookmarkModel with a real history backend.
class BookmarkModelTestWithProfile2 : public BookmarkModelTestWithProfile {
 public:
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
  }

 protected:
  // Verifies the state of the model matches that of the state in the saved
  // history file.
  void VerifyExpectedState() {
    // Here's the structure we expect:
    // bbn
    //   www.google.com - Google
    //   F1
    //     http://www.google.com/intl/en/ads/ - Google Advertising
    //     F11
    //       http://www.google.com/services/ - Google Business Solutions
    // other
    //   OF1
    //   http://www.google.com/intl/en/about.html - About Google
    BookmarkNode* bbn = bb_model_->GetBookmarkBarNode();
    ASSERT_EQ(2, bbn->GetChildCount());

    BookmarkNode* child = bbn->GetChild(0);
    ASSERT_EQ(history::StarredEntry::URL, child->GetType());
    ASSERT_EQ(L"Google", child->GetTitle());
    ASSERT_TRUE(child->GetURL() == GURL("http://www.google.com"));

    child = bbn->GetChild(1);
    ASSERT_TRUE(child->is_folder());
    ASSERT_EQ(L"F1", child->GetTitle());
    ASSERT_EQ(2, child->GetChildCount());

    BookmarkNode* parent = child;
    child = parent->GetChild(0);
    ASSERT_EQ(history::StarredEntry::URL, child->GetType());
    ASSERT_EQ(L"Google Advertising", child->GetTitle());
    ASSERT_TRUE(child->GetURL() == GURL("http://www.google.com/intl/en/ads/"));

    child = parent->GetChild(1);
    ASSERT_TRUE(child->is_folder());
    ASSERT_EQ(L"F11", child->GetTitle());
    ASSERT_EQ(1, child->GetChildCount());

    parent = child;
    child = parent->GetChild(0);
    ASSERT_EQ(history::StarredEntry::URL, child->GetType());
    ASSERT_EQ(L"Google Business Solutions", child->GetTitle());
    ASSERT_TRUE(child->GetURL() == GURL("http://www.google.com/services/"));

    parent = bb_model_->other_node();
    ASSERT_EQ(2, parent->GetChildCount());

    child = parent->GetChild(0);
    ASSERT_TRUE(child->is_folder());
    ASSERT_EQ(L"OF1", child->GetTitle());
    ASSERT_EQ(0, child->GetChildCount());

    child = parent->GetChild(1);
    ASSERT_EQ(history::StarredEntry::URL, child->GetType());
    ASSERT_EQ(L"About Google", child->GetTitle());
    ASSERT_TRUE(child->GetURL() ==
                GURL("http://www.google.com/intl/en/about.html"));

    ASSERT_TRUE(bb_model_->IsBookmarked(GURL("http://www.google.com")));
  }
};

// Tests migrating bookmarks from db into file. This copies an old history db
// file containing bookmarks and make sure they are loaded correctly and
// persisted correctly.
TEST_F(BookmarkModelTestWithProfile2, MigrateFromDBToFileTest) {
  // Copy db file over that contains starred table.
  std::wstring old_history_path;
  PathService::Get(chrome::DIR_TEST_DATA, &old_history_path);
  file_util::AppendToPath(&old_history_path, L"bookmarks");
  file_util::AppendToPath(&old_history_path, L"History_with_starred");
  std::wstring new_history_path = profile_->GetPath();
  file_util::Delete(new_history_path, true);
  file_util::CreateDirectory(new_history_path);
  file_util::AppendToPath(&new_history_path, chrome::kHistoryFilename);
  file_util::CopyFile(old_history_path, new_history_path);

  // Create the history service making sure it doesn't blow away the file we
  // just copied.
  profile_->CreateHistoryService(false);
  profile_->CreateBookmarkModel(true);
  BlockTillBookmarkModelLoaded();

  // Make sure we loaded OK.
  VerifyExpectedState();
  if (HasFatalFailure())
    return;

  // Create again. This time we shouldn't load from history at all.
  profile_->CreateBookmarkModel(false);
  BlockTillBookmarkModelLoaded();

  // Make sure we loaded OK.
  VerifyExpectedState();
  if (HasFatalFailure())
    return;

  // Recreate the history service (with a clean db). Do this just to make sure
  // we're loading correctly from the bookmarks file.
  profile_->CreateHistoryService(true);
  profile_->CreateBookmarkModel(false);
  BlockTillBookmarkModelLoaded();
  VerifyExpectedState();
}

// Simple test that removes a bookmark. This test exercises the code paths in
// History that block till bookmark bar model is loaded.
TEST_F(BookmarkModelTestWithProfile2, RemoveNotification) {
  profile_->CreateHistoryService(false);
  profile_->CreateBookmarkModel(true);
  BlockTillBookmarkModelLoaded();

  // Add a URL.
  GURL url("http://www.google.com");
  bb_model_->SetURLStarred(url, std::wstring(), true);

  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->AddPage(
      url, NULL, 1, GURL(), PageTransition::TYPED,
      HistoryService::RedirectList());

  // This won't actually delete the URL, rather it'll empty out the visits.
  // This triggers blocking on the BookmarkModel.
  profile_->GetHistoryService(Profile::EXPLICIT_ACCESS)->DeleteURL(url);
}
