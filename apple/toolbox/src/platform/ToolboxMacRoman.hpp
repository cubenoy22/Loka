#ifndef LOKA_TOOLBOX_MAC_ROMAN_HPP
#define LOKA_TOOLBOX_MAC_ROMAN_HPP

/** Projects a Unicode scalar to System 7 MacRoman; refuses unrepresentable values. */
bool ToolboxMacRomanEncode(unsigned long scalar, unsigned char &byte);

#endif
