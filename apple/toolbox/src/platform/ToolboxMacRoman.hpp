#ifndef LOKA_TOOLBOX_MAC_ROMAN_HPP
#define LOKA_TOOLBOX_MAC_ROMAN_HPP

/** Projects a Unicode scalar to System 7 MacRoman; refuses unrepresentable values. */
bool ToolboxMacRomanEncode(unsigned long scalar, unsigned char &byte);

/** Decode one System 7 MacRoman byte. All 256 bytes are defined, so this always
    succeeds; the bool mirrors ToolboxMacRomanEncode. */
bool ToolboxMacRomanDecode(unsigned char byte, unsigned long &scalar);

#endif
