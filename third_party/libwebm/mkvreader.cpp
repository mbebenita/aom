//
// Copyright (c) 2016, Alliance for Open Media. All rights reserved
//
// This source code is subject to the terms of the BSD 2 Clause License and
// the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
// was not distributed with this source code in the LICENSE file, you can
// obtain it at www.aomedia.org/license/software. If the Alliance for Open
// Media Patent License 1.0 was not distributed with this source code in the
// PATENTS file, you can obtain it at www.aomedia.org/license/patent.
//
#include "mkvreader.hpp"

#include <cassert>

namespace mkvparser {

MkvReader::MkvReader() : m_file(NULL), reader_owns_file_(true) {}

MkvReader::MkvReader(FILE* fp) : m_file(fp), reader_owns_file_(false) {
  GetFileSize();
}

MkvReader::~MkvReader() {
  if (reader_owns_file_)
    Close();
  m_file = NULL;
}

int MkvReader::Open(const char* fileName) {
  if (fileName == NULL)
    return -1;

  if (m_file)
    return -1;

#ifdef _MSC_VER
  const errno_t e = fopen_s(&m_file, fileName, "rb");

  if (e)
    return -1;  // error
#else
  m_file = fopen(fileName, "rb");

  if (m_file == NULL)
    return -1;
#endif
  return !GetFileSize();
}

bool MkvReader::GetFileSize() {
  if (m_file == NULL)
    return false;
#ifdef _MSC_VER
  int status = _fseeki64(m_file, 0L, SEEK_END);

  if (status)
    return false;  // error

  m_length = _ftelli64(m_file);
#else
  fseek(m_file, 0L, SEEK_END);
  m_length = ftell(m_file);
#endif
  assert(m_length >= 0);

  if (m_length < 0)
    return false;

#ifdef _MSC_VER
  status = _fseeki64(m_file, 0L, SEEK_SET);

  if (status)
    return false;  // error
#else
  fseek(m_file, 0L, SEEK_SET);
#endif

  return true;
}

void MkvReader::Close() {
  if (m_file != NULL) {
    fclose(m_file);
    m_file = NULL;
  }
}

int MkvReader::Length(long long* total, long long* available) {
  if (m_file == NULL)
    return -1;

  if (total)
    *total = m_length;

  if (available)
    *available = m_length;

  return 0;
}

int MkvReader::Read(long long offset, long len, unsigned char* buffer) {
  if (m_file == NULL)
    return -1;

  if (offset < 0)
    return -1;

  if (len < 0)
    return -1;

  if (len == 0)
    return 0;

  if (offset >= m_length)
    return -1;

#ifdef _MSC_VER
  const int status = _fseeki64(m_file, offset, SEEK_SET);

  if (status)
    return -1;  // error
#else
  fseek(m_file, offset, SEEK_SET);
#endif

  const size_t size = fread(buffer, 1, len, m_file);

  if (size < size_t(len))
    return -1;  // error

  return 0;  // success
}

}  // end namespace mkvparser