// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "chrome/browser/webdata/web_database.h"

#include "base/gfx/png_decoder.h"
#include "base/gfx/png_encoder.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/ie7_password.h"
#include "chrome/browser/template_url.h"
#include "chrome/browser/encryptor.h"
#include "chrome/common/scoped_vector.h"
#include "webkit/glue/password_form.h"

////////////////////////////////////////////////////////////////////////////////
//
// Schema
//
// keywords                 Most of the columns mirror that of a field in
//                          TemplateURL. See TemplateURL for more details.
//   id
//   short_name
//   keyword
//   favicon_url
//   url
//   show_in_default_list
//   safe_for_autoreplace
//   originating_url
//   date_created           This column was added after we allowed keywords.
//                          Keywords created before we started tracking
//                          creation date have a value of 0 for this.
//   usage_count
//   input_encodings        Semicolon separated list of supported input
//                          encodings, may be empty.
//   suggest_url
//   prepopulate_id         See TemplateURL::prepoulate_id.
//   autogenerate_keyword
//
// logins
//   origin_url
//   action_url
//   username_element
//   username_value
//   password_element
//   password_value
//   submit_element
//   signon_realm        The authority (scheme, host, port).
//   ssl_valid           SSL status of page containing the form at first
//                       impression.
//   preferred           MRU bit.
//   date_created        This column was added after logins support. "Legacy"
//                       entries have a value of 0.
//   blacklisted_by_user Tracks whether or not the user opted to 'never
//                       remember'
//                       passwords for this site.
//
// web_app_icons
//   url         URL of the web app.
//   width       Width of the image.
//   height      Height of the image.
//   image       PNG encoded image data.
//
// web_apps
//   url                 URL of the web app.
//   has_all_images      Do we have all the images?
//
////////////////////////////////////////////////////////////////////////////////

// Current version number.
static const int kCurrentVersionNumber = 21;
static const int kCompatibleVersionNumber = 21;

// Keys used in the meta table.
static const char* kDefaultSearchProviderKey = "Default Search Provider ID";
static const char* kBuiltinKeywordVersion = "Builtin Keyword Version";

std::string JoinStrings(const std::string& separator,
                        const std::vector<std::string>& strings) {
  if (strings.empty())
    return std::string();
  std::vector<std::string>::const_iterator i(strings.begin());
  std::string result(*i);
  while(++i != strings.end())
    result += separator + *i;
  return result;
}

WebDatabase::WebDatabase() : db_(NULL), transaction_nesting_(0) {
}

WebDatabase::~WebDatabase() {
  if (db_) {
    DCHECK(transaction_nesting_ == 0) <<
        "Forgot to close the transaction on shutdown";
    sqlite3_close(db_);
    db_ = NULL;
  }
}

void WebDatabase::BeginTransaction() {
  DCHECK(db_);
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "BEGIN TRANSACTION", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to begin transaction";
  }
  transaction_nesting_++;
}

void WebDatabase::CommitTransaction() {
  DCHECK(db_);
  DCHECK(transaction_nesting_ > 0) << "Committing too many transaction";
  transaction_nesting_--;
  if (transaction_nesting_ == 0) {
    int rv = sqlite3_exec(db_, "COMMIT", NULL, NULL, NULL);
    DCHECK(rv == SQLITE_OK) << "Failed to commit transaction";
  }
}

bool WebDatabase::Init(const std::wstring& db_name) {
  // Open the database, using the narrow version of open so that
  // the DB is in UTF-8.
  if (sqlite3_open(WideToUTF8(db_name).c_str(), &db_) != SQLITE_OK) {
    LOG(WARNING) << "Unable to open the web database.";
    return false;
  }

  // We don't store that much data in the tables so use a small page size.
  // This provides a large benefit for empty tables (which is very likely with
  // the tables we create).
  sqlite3_exec(db_, "PRAGMA page_size=2048", NULL, NULL, NULL);

  // We shouldn't have much data and what access we currently have is quite
  // infrequent. So we go with a small cache size.
  sqlite3_exec(db_, "PRAGMA cache_size=32", NULL, NULL, NULL);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  sqlite3_exec(db_, "PRAGMA locking_mode=EXCLUSIVE", NULL, NULL, NULL);

  // Initialize various tables
  SQLTransaction transaction(db_);
  transaction.Begin();

  // Version check.
  if (!meta_table_.Init(std::string(), kCurrentVersionNumber,
                        kCompatibleVersionNumber, db_))
    return false;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Web database is too new.";
    return false;
  }

  // Initialize the tables.
  if (!InitKeywordsTable() || !InitLoginsTable() || !InitWebAppIconsTable() ||
      !InitWebAppsTable()) {
    LOG(WARNING) << "Unable to initialize the web database.";
    return false;
  }

  // If the file on disk is an older database version, bring it up to date.
  MigrateOldVersionsAsNeeded();

  return (transaction.Commit() == SQLITE_OK);
}

bool WebDatabase::SetWebAppImage(const GURL& url,
                                 const SkBitmap& image) {
  SQLStatement s;
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO web_app_icons "
                "(url, width, height, image) VALUES (?, ?, ?, ?)")
      != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  std::vector<unsigned char> image_data;

  SkAutoLockPixels pixel_lock(image);
  PNGEncoder::Encode(reinterpret_cast<unsigned char*>(image.getPixels()),
                     PNGEncoder::FORMAT_BGRA, image.width(),
                     image.height(), image.width() * 4, false, &image_data);

  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.bind_int(1, image.width());
  s.bind_int(2, image.height());
  s.bind_blob(3, &image_data.front(), static_cast<int>(image_data.size()));
  return s.step() == SQLITE_DONE;
}

bool WebDatabase::GetWebAppImages(const GURL& url,
                                  std::vector<SkBitmap>* images) {
  SQLStatement s;
  if (s.prepare(db_, "SELECT image FROM web_app_icons WHERE url=?") !=
      SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  while (s.step() == SQLITE_ROW) {
    SkBitmap image;
    std::vector<unsigned char> image_data;
    s.column_blob_as_vector(0, &image_data);
    if (PNGDecoder::Decode(&image_data, &image)) {
      images->push_back(image);
    } else {
      // Should only have valid image data in the db.
      NOTREACHED();
    }
  }
  return true;
}

bool WebDatabase::SetWebAppHasAllImages(const GURL& url,
                                        bool has_all_images) {
  SQLStatement s;
  if (s.prepare(db_, "INSERT OR REPLACE INTO web_apps (url, has_all_images) "
                     "VALUES (?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  s.bind_int(1, has_all_images ? 1 : 0);
  return (s.step() == SQLITE_DONE);
}

bool WebDatabase::GetWebAppHasAllImages(const GURL& url) {
  SQLStatement s;
  if (s.prepare(db_, "SELECT has_all_images FROM web_apps "
                     "WHERE url=?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (s.step() == SQLITE_ROW && s.column_int(0) == 1);
}

bool WebDatabase::RemoveWebApp(const GURL& url) {
  SQLStatement delete_s;
  if (delete_s.prepare(db_, "DELETE FROM web_app_icons WHERE url = ?") !=
      SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  if (delete_s.step() != SQLITE_DONE)
    return false;

  SQLStatement delete_s2;
  if (delete_s2.prepare(db_, "DELETE FROM web_apps WHERE url = ?") !=
      SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  delete_s2.bind_string(0, history::HistoryDatabase::GURLToDatabaseURL(url));
  return (delete_s2.step() == SQLITE_DONE);
}

bool WebDatabase::InitKeywordsTable() {
  if (!DoesSqliteTableExist(db_, "keywords")) {
    if (sqlite3_exec(db_, "CREATE TABLE keywords ("
                     "id INTEGER PRIMARY KEY,"
                     "short_name VARCHAR NOT NULL,"
                     "keyword VARCHAR NOT NULL,"
                     "favicon_url VARCHAR NOT NULL,"
                     "url VARCHAR NOT NULL,"
                     "show_in_default_list INTEGER,"
                     "safe_for_autoreplace INTEGER,"
                     "originating_url VARCHAR,"
                     "date_created INTEGER DEFAULT 0,"
                     "usage_count INTEGER DEFAULT 0,"
                     "input_encodings VARCHAR,"
                     "suggest_url VARCHAR,"
                     "prepopulate_id INTEGER DEFAULT 0,"
                     "autogenerate_keyword INTEGER DEFAULT 0)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitLoginsTable() {
  if (!DoesSqliteTableExist(db_, "logins")) {
    // First time
    if (sqlite3_exec(db_, "CREATE TABLE logins ("
                     "origin_url VARCHAR NOT NULL, "
                     "action_url VARCHAR, "
                     "username_element VARCHAR, "
                     "username_value VARCHAR, "
                     "password_element VARCHAR, "
                     "password_value BLOB, "
                     "submit_element VARCHAR, "
                     "signon_realm VARCHAR NOT NULL,"
                     "ssl_valid INTEGER NOT NULL,"
                     "preferred INTEGER NOT NULL,"
                     "date_created INTEGER NOT NULL,"
                     "blacklisted_by_user INTEGER NOT NULL,"
                     "scheme INTEGER NOT NULL,"
                     "UNIQUE "
                     "(origin_url, username_element, "
                     "username_value, password_element, "
                     "submit_element, signon_realm))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX logins_signon ON "
                     "logins (signon_realm)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }

  if (!DoesSqliteTableExist(db_, "ie7_logins")) {
    // First time
    if (sqlite3_exec(db_, "CREATE TABLE ie7_logins ("
                     "url_hash VARCHAR NOT NULL, "
                     "password_value BLOB, "
                     "date_created INTEGER NOT NULL,"
                     "UNIQUE "
                     "(url_hash))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX ie7_logins_hash ON "
                     "ie7_logins (url_hash)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppIconsTable() {
  if (!DoesSqliteTableExist(db_, "web_app_icons")) {
    if (sqlite3_exec(db_, "CREATE TABLE web_app_icons ("
                     "url LONGVARCHAR,"
                     "width int,"
                     "height int,"
                     "image BLOB, UNIQUE (url, width, height))",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

bool WebDatabase::InitWebAppsTable() {
  if (!DoesSqliteTableExist(db_, "web_apps")) {
    if (sqlite3_exec(db_, "CREATE TABLE web_apps ("
                     "url LONGVARCHAR UNIQUE,"
                     "has_all_images INTEGER NOT NULL)",
                     NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
    if (sqlite3_exec(db_, "CREATE INDEX web_apps_url_index ON "
                     "web_apps (url)", NULL, NULL, NULL) != SQLITE_OK) {
      NOTREACHED();
      return false;
    }
  }
  return true;
}

static void BindURLToStatement(const TemplateURL& url, SQLStatement* s) {
  s->bind_wstring(0, url.short_name());
  s->bind_wstring(1, url.keyword());
  GURL favicon_url = url.GetFavIconURL();
  if (!favicon_url.is_valid()) {
    s->bind_string(2, "");
  } else {
    s->bind_string(2, history::HistoryDatabase::GURLToDatabaseURL(
                       url.GetFavIconURL()));
  }
  if (url.url())
    s->bind_wstring(3, url.url()->url());
  else
    s->bind_wstring(3, std::wstring());
  s->bind_int(4, url.safe_for_autoreplace() ? 1 : 0);
  if (!url.originating_url().is_valid()) {
    s->bind_string(5, std::string());
  } else {
    s->bind_string(5, history::HistoryDatabase::GURLToDatabaseURL(
        url.originating_url()));
  }
  s->bind_int64(6, url.date_created().ToTimeT());
  s->bind_int(7, url.usage_count());
  s->bind_string(8, JoinStrings(";", url.input_encodings()));
  s->bind_int(9, url.show_in_default_list() ? 1 : 0);
  if (url.suggestions_url())
    s->bind_wstring(10, url.suggestions_url()->url());
  else
    s->bind_wstring(10, std::wstring());
  s->bind_int(11, url.prepopulate_id());
  s->bind_int(12, url.autogenerate_keyword() ? 1 : 0);
}

bool WebDatabase::AddKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  SQLStatement s;
  if (s.prepare(db_,
                "INSERT INTO keywords "
                "(short_name, keyword, favicon_url, url, safe_for_autoreplace, "
                "originating_url, date_created, usage_count, input_encodings, "
                "show_in_default_list, suggest_url, prepopulate_id, "
                "autogenerate_keyword, id) VALUES "
                "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.bind_int64(13, url.id());
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveKeyword(TemplateURL::IDType id) {
  DCHECK(id);
  SQLStatement s;
  if (s.prepare(db_,
                "DELETE FROM keywords WHERE id = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_int64(0, id);
  return s.step() == SQLITE_DONE;
}

bool WebDatabase::GetKeywords(std::vector<TemplateURL*>* urls) {
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT id, short_name, keyword, favicon_url, url, "
                "safe_for_autoreplace, originating_url, date_created, "
                "usage_count, input_encodings, show_in_default_list, "
                "suggest_url, prepopulate_id, autogenerate_keyword "
                "FROM keywords ORDER BY id ASC") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  int64 result;
  while ((result = s.step()) == SQLITE_ROW) {
    TemplateURL* template_url = new TemplateURL();
    std::wstring tmp;
    template_url->set_id(s.column_int64(0));

    s.column_string16(1, &tmp);
    DCHECK(!tmp.empty());
    template_url->set_short_name(tmp);

    s.column_string16(2, &tmp);
    template_url->set_keyword(tmp);

    s.column_string16(3, &tmp);
    if (!tmp.empty())
      template_url->SetFavIconURL(GURL(tmp));

    s.column_string16(4, &tmp);
    template_url->SetURL(tmp, 0, 0);

    template_url->set_safe_for_autoreplace(s.column_int(5) == 1);

    s.column_string16(6, &tmp);
    if (!tmp.empty())
      template_url->set_originating_url(GURL(tmp));

    template_url->set_date_created(Time::FromTimeT(s.column_int64(7)));

    template_url->set_usage_count(s.column_int(8));

    std::vector<std::string> encodings;
    SplitString(s.column_string(9), ';', &encodings);
    template_url->set_input_encodings(encodings);

    template_url->set_show_in_default_list(s.column_int(10) == 1);

    s.column_string16(11, &tmp);
    template_url->SetSuggestionsURL(tmp, 0, 0);

    template_url->set_prepopulate_id(s.column_int(12));

    template_url->set_autogenerate_keyword(s.column_int(13) == 1);

    urls->push_back(template_url);
  }
  return result == SQLITE_DONE;
}

bool WebDatabase::UpdateKeyword(const TemplateURL& url) {
  DCHECK(url.id());
  SQLStatement s;
  if (s.prepare(db_,
                "UPDATE keywords "
                "SET short_name=?, keyword=?, favicon_url=?, url=?, "
                "safe_for_autoreplace=?, originating_url=?, date_created=?, "
                "usage_count=?, input_encodings=?, show_in_default_list=?, "
                "suggest_url=?, prepopulate_id=?, autogenerate_keyword=? "
                "WHERE id=?")
                != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  BindURLToStatement(url, &s);
  s.bind_int64(13, url.id());
  return s.step() == SQLITE_DONE;
}

bool WebDatabase::SetDefaultSearchProviderID(int64 id) {
  return meta_table_.SetValue(kDefaultSearchProviderKey, id);
}

int64 WebDatabase::GetDefaulSearchProviderID() {
  int64 value = 0;
  meta_table_.GetValue(kDefaultSearchProviderKey, &value);
  return value;
}

bool WebDatabase::SetBuitinKeywordVersion(int version) {
  return meta_table_.SetValue(kBuiltinKeywordVersion, version);
}

int WebDatabase::GetBuitinKeywordVersion() {
  int version = 0;
  meta_table_.GetValue(kBuiltinKeywordVersion, &version);
  return version;
}

// Return a new GURL like url, but without any "#foo" bit on the end.
static GURL GURLWithoutRef(const GURL& url) {
  url_canon::Replacements<char> replacements;
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// Convert a list of GUIDs from the in-memory form to the form we keep in
// the database (tab-separated string).
static std::string SerializeGUIDs(const std::vector<std::string>& guids) {
  std::string result;
  for (size_t i = 0; i < guids.size(); ++i) {
    if (!result.empty())
      result.push_back('\t');
    const std::string& guid = guids[i];
    for (size_t j = 0; j < guid.size(); ++j) {
      char ch = guid[j];
      // If we have any embedded tabs in the GUID (a pathological case),
      // we normalize them to spaces.
      if (ch == '\t')
        ch = ' ';
      result.push_back(ch);
    }
  }
  return result;
}

// The partner of SerializeGUIDs.  Converts a serialized GUIDs string
// back to a vector.
static void DeserializeGUIDs(const std::string& str,
                             std::vector<std::string>* guids) {
  SplitString(str, '\t', guids);
}

bool WebDatabase::AddLogin(const PasswordForm& form) {
  SQLStatement s;
  std::string encrypted_password;
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO logins "
                "(origin_url, action_url, username_element, username_value, "
                " password_element, password_value, submit_element, "
                " signon_realm, ssl_valid, preferred, date_created, "
                " blacklisted_by_user, scheme) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.origin.spec());
  s.bind_string(1, form.action.spec());
  s.bind_wstring(2, form.username_element);
  s.bind_wstring(3, form.username_value);
  s.bind_wstring(4, form.password_element);
  Encryptor::EncryptWideString(form.password_value, &encrypted_password);
  s.bind_blob(5, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.bind_wstring(6, form.submit_element);
  s.bind_string(7, form.signon_realm);
  s.bind_int(8, form.ssl_valid);
  s.bind_int(9, form.preferred);
  s.bind_int64(10, form.date_created.ToTimeT());
  s.bind_int(11, form.blacklisted_by_user);
  s.bind_int(12, form.scheme);
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::AddIE7Login(const IE7PasswordInfo& info) {
  SQLStatement s;
  if (s.prepare(db_,
                "INSERT OR REPLACE INTO ie7_logins "
                "(url_hash, password_value, date_created) "
                "VALUES (?, ?, ?)") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_wstring(0, info.url_hash);
  s.bind_blob(1, &info.encrypted_data.front(),
              static_cast<int>(info.encrypted_data.size()));
  s.bind_int64(2, info.date_created.ToTimeT());
  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::UpdateLogin(const PasswordForm& form) {
  SQLStatement s;
  std::string encrypted_password;
  if (s.prepare(db_, "UPDATE logins SET "
                "action_url = ?, "
                "password_value = ?, "
                "ssl_valid = ?, "
                "preferred = ? "
                "WHERE origin_url = ? AND "
                "username_element = ? AND "
                "username_value = ? AND "
                "password_element = ? AND "
                "signon_realm = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.action.spec());
  Encryptor::EncryptWideString(form.password_value, &encrypted_password);
  s.bind_blob(1, encrypted_password.data(),
              static_cast<int>(encrypted_password.length()));
  s.bind_int(2, form.ssl_valid);
  s.bind_int(3, form.preferred);
  s.bind_string(4, form.origin.spec());
  s.bind_wstring(5, form.username_element);
  s.bind_wstring(6, form.username_value);
  s.bind_wstring(7, form.password_element);
  s.bind_string(8, form.signon_realm);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLogin(const PasswordForm& form) {
  SQLStatement s;
  // Remove a login by UNIQUE-constrained fields.
  if (s.prepare(db_,
                "DELETE FROM logins WHERE "
                "origin_url = ? AND "
                "username_element = ? AND "
                "username_value = ? AND "
                "password_element = ? AND "
                "submit_element = ? AND "
                "signon_realm = ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_string(0, form.origin.spec());
  s.bind_wstring(1, form.username_element);
  s.bind_wstring(2, form.username_value);
  s.bind_wstring(3, form.password_element);
  s.bind_wstring(4, form.submit_element);
  s.bind_string(5, form.signon_realm);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveIE7Login(const IE7PasswordInfo& info) {
  SQLStatement s;
  // Remove a login by UNIQUE-constrained fields.
  if (s.prepare(db_,
                "DELETE FROM ie7_logins WHERE "
                "url_hash = ?") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }
  s.bind_wstring(0, info.url_hash);

  if (s.step() != SQLITE_DONE) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool WebDatabase::RemoveLoginsCreatedBetween(const Time delete_begin,
                                             const Time delete_end) {
  SQLStatement s1;
  if (s1.prepare(db_,
                "DELETE FROM logins WHERE "
                "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement 1 prepare failed";
    return false;
  }
  s1.bind_int64(0, delete_begin.ToTimeT());
  s1.bind_int64(1,
                delete_end.is_null() ?
                    std::numeric_limits<int64>::max() :
                    delete_end.ToTimeT());

  SQLStatement s2;
  if (s2.prepare(db_,
               "DELETE FROM ie7_logins WHERE "
               "date_created >= ? AND date_created < ?") != SQLITE_OK) {
    NOTREACHED() << "Statement 2 prepare failed";
    return false;
  }
  s2.bind_int64(0, delete_begin.ToTimeT());
  s2.bind_int64(1,
                delete_end.is_null() ?
                    std::numeric_limits<int64>::max() :
                    delete_end.ToTimeT());

  return s1.step() == SQLITE_DONE && s2.step() == SQLITE_DONE;
}

static void InitPasswordFormFromStatement(PasswordForm* form,
                                          SQLStatement* s) {
  std::string encrypted_password;
  std::string tmp;
  s->column_string(0, &tmp);
  form->origin = GURL(tmp);
  s->column_string(1, &tmp);
  form->action = GURL(tmp);
  s->column_string16(2, &form->username_element);
  s->column_string16(3, &form->username_value);
  s->column_string16(4, &form->password_element);
  s->column_blob_as_string(5, &encrypted_password);
  Encryptor::DecryptWideString(encrypted_password, &form->password_value);
  s->column_string16(6, &form->submit_element);
  s->column_string(7, &tmp);
  form->signon_realm = tmp;
  form->ssl_valid = (s->column_int(8) > 0);
  form->preferred = (s->column_int(9) > 0);
  form->date_created = Time::FromTimeT(s->column_int64(10));
  form->blacklisted_by_user = (s->column_int(11) > 0);
  int scheme_int = s->column_int(12);
  DCHECK((scheme_int >= 0) && (scheme_int <= PasswordForm::SCHEME_OTHER));
  form->scheme = static_cast<PasswordForm::Scheme>(scheme_int);
}

bool WebDatabase::GetLogins(const PasswordForm& form,
                            std::vector<PasswordForm*>* forms) {
  DCHECK(forms);
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT origin_url, action_url, "
                "username_element, username_value, "
                "password_element, password_value, "
                "submit_element, signon_realm, "
                "ssl_valid, preferred, "
                "date_created, blacklisted_by_user, scheme FROM logins "
                "WHERE signon_realm == ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_string(0, form.signon_realm);

  int64 result;
  while ((result = s.step()) == SQLITE_ROW) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return result == SQLITE_DONE;
}

bool WebDatabase::GetIE7Login(const IE7PasswordInfo& info,
                              IE7PasswordInfo* result) {
  DCHECK(result);
  SQLStatement s;
  if (s.prepare(db_,
                "SELECT password_value, date_created FROM ie7_logins "
                "WHERE url_hash == ? ") != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  s.bind_wstring(0, info.url_hash);

  int64 query_result = s.step();
  if (query_result == SQLITE_ROW) {
    s.column_blob_as_vector(0, &result->encrypted_data);
    result->date_created = Time::FromTimeT(s.column_int64(1));
    result->url_hash = info.url_hash;
    s.step();
  }
  return query_result == SQLITE_DONE;
}

bool WebDatabase::GetAllLogins(std::vector<PasswordForm*>* forms,
                               bool include_blacklisted) {
  DCHECK(forms);
  SQLStatement s;
  std::string stmt = "SELECT origin_url, action_url, "
                     "username_element, username_value, "
                     "password_element, password_value, "
                     "submit_element, signon_realm, ssl_valid, preferred, "
                     "date_created, blacklisted_by_user, scheme FROM logins ";
  if (!include_blacklisted)
    stmt.append("WHERE blacklisted_by_user == 0 ");
  stmt.append("ORDER BY origin_url");

  if (s.prepare(db_, stmt.c_str()) != SQLITE_OK) {
    NOTREACHED() << "Statement prepare failed";
    return false;
  }

  int64 result;
  while ((result = s.step()) == SQLITE_ROW) {
    PasswordForm* new_form = new PasswordForm();
    InitPasswordFormFromStatement(new_form, &s);

    forms->push_back(new_form);
  }
  return result == SQLITE_DONE;
}

void WebDatabase::MigrateOldVersionsAsNeeded() {
  // Migrate if necessary.
  int current_version = meta_table_.GetVersionNumber();
  switch(current_version) {
    // Versions 1 - 19 are unhandled.  Version numbers greater than
    // kCurrentVersionNumber should have already been weeded out by the caller.
    default:
      // When the version is too old, we just try to continue anyway.  There
      // should not be a released product that makes a database too old for us
      // to handle.
      LOG(WARNING) << "Web database version " << current_version <<
          " is too old to handle.";
      return;

    case 20:
      // Add the autogenerate_keyword column.
      if (sqlite3_exec(db_,
                       "ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                       "INTEGER DEFAULT 0", NULL, NULL, NULL) != SQLITE_OK) {
        NOTREACHED();
        LOG(WARNING) << "Unable to update web database to version 21.";
        return;
      }
      meta_table_.SetVersionNumber(21);
      meta_table_.SetCompatibleVersionNumber(
          std::min(21, kCompatibleVersionNumber));
      // FALL THROUGH

    // Add successive versions here.  Each should set the version number and
    // compatible version number as appropriate, then fall through to the next
    // case.

    case kCurrentVersionNumber:
      // No migration needed.
      return;
  }
}

