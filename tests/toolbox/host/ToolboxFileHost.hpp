#ifndef LOKA_TEST_TOOLBOX_FILE_HOST_HPP
#define LOKA_TEST_TOOLBOX_FILE_HOST_HPP
#include "Files.h"
#include <cstddef>
#include <string>
namespace toolbox_file_host
{
  FSSpec Spec(short volume, int32_t parent, const std::string &name);
  void Put(const FSSpec &spec, const std::string &contents);
  void SetApplication(const FSSpec &spec);
  std::size_t OpenCount();
}
#endif
