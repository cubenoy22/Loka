#ifndef LOKA_TEST_TOOLBOX_FILE_HOST_HPP
#define LOKA_TEST_TOOLBOX_FILE_HOST_HPP
#include "Files.h"
#include <cstddef>
#include <string>
namespace toolbox_file_host
{
  FSSpec Spec(short volume, int32_t parent, const std::string &name);
  void Put(const FSSpec &spec, const std::string &contents);
  void SetMetadata(const FSSpec &spec, OSType type, OSType creator);
  FInfo Metadata(const FSSpec &spec);
  void FailPrepare(OSErr catalog, OSErr create);
  unsigned CatalogCalls();
  unsigned CreateCalls();
  ScriptCode CreatedScript();
  void Remove(const FSSpec &spec);
  enum ReadFailure { NoFailure, SizeFailure, DataFailure };
  void FailRead(ReadFailure failure);
  void SetApplication(const FSSpec &spec);
  std::size_t OpenCount();
}
#endif
