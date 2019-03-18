#pragma once

namespace CobbBugFixes {
   struct INISetting;
   //
   // SETTING DEFINITIONS -- BEGIN
   //
   #define COBBBUGFIXES_MAKE_INI_SETTING(category, name, value) namespace category { extern INISetting name; };
   namespace INI {
      COBBBUGFIXES_MAKE_INI_SETTING(UnderwaterAmbienceCellBoundaryFix, Enabled, true);
   };
   #undef COBBBUGFIXES_MAKE_INI_SETTING
   //
   // SETTING DEFINITIONS -- END
   //
   // INTERNALS BELOW
   //
   class INISettingManager {
      public:
         static INISettingManager& GetInstance();
         //
         void Load();
         void Save();
   };
   struct INISetting {
      enum ValueType {
         kType_Bool  = 0,
         kType_Float = 1,
         kType_SInt  = 2,
         kType_UInt  = 3,
      };
      INISetting(const char* n, const char* c, bool   b) : type(kType_Bool),  name(n), category(c), bDefault(b), bCurrent(b) {};
      INISetting(const char* n, const char* c, float  f) : type(kType_Float), name(n), category(c), fDefault(f), fCurrent(f) {};
      INISetting(const char* n, const char* c, SInt32 i) : type(kType_SInt),  name(n), category(c), iDefault(i), iCurrent(i) {};
      INISetting(const char* n, const char* c, UInt32 u) : type(kType_UInt),  name(n), category(c), uDefault(u), uCurrent(u) {};
      //
      const char* const name;
      const char* const category;
      const ValueType type;
      union {
         bool   bDefault;
         float  fDefault;
         SInt32 iDefault;
         UInt32 uDefault;
      };
      union {
         bool   bCurrent;
         float  fCurrent;
         SInt32 iCurrent;
         UInt32 uCurrent;
      };
      //
      void ToString(std::string& out) const;
   };
};