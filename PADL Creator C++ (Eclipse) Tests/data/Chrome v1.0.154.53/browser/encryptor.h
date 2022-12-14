// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// A class for encrypting/decrypting strings

#ifndef CHROME_BROWSER_ENCRYPTOR_H__
#define CHROME_BROWSER_ENCRYPTOR_H__

#include <string>

#include "base/values.h"

class Encryptor {
public:
  // Encrypt a wstring. The output (second argument) is
  // really an array of bytes, but we're passing it back
  // as a std::string
  static bool EncryptWideString(const std::wstring& plaintext,
                                std::string* ciphertext);

  // Decrypt an array of bytes obtained with EnctryptWideString
  // back into a wstring. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptWideString(const std::string& ciphertext,
                                std::wstring* plaintext);

  // Encrypt a string.
  static bool EncryptString(const std::string& plaintext,
                            std::string* ciphertext);

  // Decrypt an array of bytes obtained with EnctryptString
  // back into a string. Note that the input (first argument)
  // is a std::string, so you need to first get your (binary)
  // data into a string.
  static bool DecryptString(const std::string& ciphertext,
                            std::string* plaintext);

private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Encryptor);
};

#endif  // CHROME_BROWSER_ENCRYPTOR_H__

