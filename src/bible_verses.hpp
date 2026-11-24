#pragma once

#include <cstddef>
#include <string>
#include <vector>

int BibleVerseCount();

// 1-based; returns nullptr for an invalid id.
const char* BibleVerse(int verseId);

std::vector<std::string> WrapVerseText(const char* text, size_t maxLineLength);
