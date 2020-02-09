#include "CrashLogDefinitions.h"

const CrashLogLabel g_labels[] = {
   CrashLogLabel(0x010CFCBC, 0x4A0, "Actor", CrashLogLabel::Type::vtbl),
};
const UInt32 g_labelCount = std::extent<decltype(g_labels)>::value;