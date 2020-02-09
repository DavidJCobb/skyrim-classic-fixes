#pragma once

struct CrashLogLabel {
   enum class Type {
      subroutine,
      address,
      vtbl,
      constant,
   };
   //
   const char* name;
   UInt32 start;
   UInt16 size;
   Type   type = Type::address;
   //
   CrashLogLabel(UInt32 b, UInt16 c, const char* a) : name(a), start(b), size(c) {};
   CrashLogLabel(UInt32 b, UInt16 c, const char* a, Type d) : name(a), start(b), size(c), type(d) {};
};
extern const CrashLogLabel g_labels[]; // std::extent doesn't work from outside of the CPP file
extern const UInt32 g_labelCount;