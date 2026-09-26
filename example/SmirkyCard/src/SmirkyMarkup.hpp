#ifndef SMIRKYCARD_SMIRKY_MARKUP_HPP
#define SMIRKYCARD_SMIRKY_MARKUP_HPP

#include "app/style/AttributedString.hpp"

namespace smirkycard
{
  /** Parses UTF-8 markup into flattened runs. Failure leaves out unchanged. */
  bool ParseSmirkyMarkup(const char *bytes,
                         std::size_t length,
                         const loka::app::TextStyle &base,
                         loka::app::AttributedString &out);
} // namespace smirkycard

#endif
