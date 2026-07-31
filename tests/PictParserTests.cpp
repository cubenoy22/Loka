#include "PictParserTests.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

#include "../apple/toolbox/src/PictParser.hpp"

namespace
{
  static void PutU16BE(std::vector<unsigned char> &bytes,
                       std::size_t offset,
                       unsigned short value)
  {
    bytes[offset] = static_cast<unsigned char>((value >> 8) & 0xFF);
    bytes[offset + 1] = static_cast<unsigned char>(value & 0xFF);
  }

  static void PutPlausibleHeader(std::vector<unsigned char> &bytes,
                                 std::size_t offset,
                                 unsigned short size)
  {
    PutU16BE(bytes, offset, size);
    PutU16BE(bytes, offset + 2, 2);
    PutU16BE(bytes, offset + 4, 3);
    PutU16BE(bytes, offset + 6, 12);
    PutU16BE(bytes, offset + 8, 23);
    bytes[offset + 10] = 0;
    bytes[offset + 11] = 0;
    bytes[offset + 12] = 0;
    bytes[offset + 13] = 0;
  }

  static void PutVersion1Pict(std::vector<unsigned char> &bytes,
                              std::size_t offset)
  {
    PutPlausibleHeader(bytes, offset, 14);
    bytes[offset + 10] = 0x11;
    bytes[offset + 11] = 0x01;
    bytes[offset + 12] = 0x00;
    bytes[offset + 13] = 0xFF;
  }

  static void PutVersion2Pict(std::vector<unsigned char> &bytes,
                              std::size_t offset)
  {
    PutPlausibleHeader(bytes, offset, 16);
    bytes[offset + 10] = 0x00;
    bytes[offset + 11] = 0x11;
    bytes[offset + 12] = 0x02;
    bytes[offset + 13] = 0xFF;
    bytes[offset + 14] = 0x00;
    bytes[offset + 15] = 0xFF;
  }
}

void testPictParserKeepsRawRangeAtItsBase()
{
  const std::size_t rangeBase = 7;
  std::vector<unsigned char> bytes(rangeBase + 14, 0xA5);
  PutVersion1Pict(bytes, rangeBase);

  loka::toolbox::pict::PictParseResult result;
  assert(loka::toolbox::pict::ParsePict(
      bytes, rangeBase, bytes.size(), result));
  assert(result.pictureOffset == rangeBase);
  assert(result.pictureSize == 14);
  assert(result.width == 20);
  assert(result.height == 10);

  std::printf("testPictParserKeepsRawRangeAtItsBase passed\n");
}

void testPictParserAcceptsOrdinaryHeaderedFile()
{
  std::vector<unsigned char> bytes(512 + 16, 0);
  PutVersion2Pict(bytes, 512);

  loka::toolbox::pict::PictParseResult result;
  assert(loka::toolbox::pict::ParsePict(
      bytes, 0, bytes.size(), result));
  assert(result.pictureOffset == 512);
  assert(result.pictureSize == 16);

  std::printf("testPictParserAcceptsOrdinaryHeaderedFile passed\n");
}

void testPictParserCorroboratesDeceptiveHeaderBeforeChoosingOffset()
{
  // An application header may begin with ten bytes that look exactly like a
  // nonempty PICT header. The real versioned stream still starts at 512.
  std::vector<unsigned char> bytes(512 + 16, 0);
  PutPlausibleHeader(bytes, 0, 16);
  PutVersion2Pict(bytes, 512);

  loka::toolbox::pict::PictParseResult result;
  assert(loka::toolbox::pict::ParsePict(
      bytes, 0, bytes.size(), result));
  assert(result.pictureOffset == 512);

  // If the application header also happens to carry a version opcode, the
  // headered-file candidate wins the corroborated tie.
  PutVersion2Pict(bytes, 0);
  assert(loka::toolbox::pict::ParsePict(
      bytes, 0, bytes.size(), result));
  assert(result.pictureOffset == 512);

  // Corroboration works in both directions: a versioned raw stream beats an
  // merely plausible candidate 512 bytes later.
  PutVersion1Pict(bytes, 0);
  PutPlausibleHeader(bytes, 512, 16);
  assert(loka::toolbox::pict::ParsePict(
      bytes, 0, bytes.size(), result));
  assert(result.pictureOffset == 0);

  // Plausible frame bytes alone are never sufficient corroboration.
  std::vector<unsigned char> unversioned(16, 0);
  PutPlausibleHeader(unversioned, 0, 16);
  assert(!loka::toolbox::pict::ParsePict(
      unversioned, 0, unversioned.size(), result));

  std::printf(
      "testPictParserCorroboratesDeceptiveHeaderBeforeChoosingOffset passed\n");
}
