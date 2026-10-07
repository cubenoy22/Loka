#ifndef LOKA_TEST_STANDARD_FILE_H
#define LOKA_TEST_STANDARD_FILE_H
#include "Files.h"
struct StandardFileReply { bool sfGood; FSSpec sfFile; };
void StandardPutFile(const unsigned char *, const unsigned char *, StandardFileReply *);
void StandardGetFile(void *, short, void *, StandardFileReply *);
#endif
