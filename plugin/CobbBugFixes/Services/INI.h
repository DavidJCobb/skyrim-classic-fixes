#pragma once
#include <unordered_map>
#include <vector>

namespace CobbBugFixes {
   struct INISetting;
   //
   // SETTING DEFINITIONS -- BEGIN
   //
   #define COBBBUGFIXES_MAKE_INI_SETTING(category, name, value) namespace category { extern INISetting name; };
   namespace INI {
      COBBBUGFIXES_MAKE_INI_SETTING(MerchantRestockFixes,              Enabled, true);
      COBBBUGFIXES_MAKE_INI_SETTING(UnderwaterAmbienceCellBoundaryFix, Enabled, true);
   };
   #undef COBBBUGFIXES_MAKE_INI_SETTING
   //
   // SETTING DEFINITIONS -- END
   //
   // INTERNALS BELOW
   //
   class INISettingManager {
      private:
         typedef std::vector<INISetting*> VecSettings;
         typedef std::unordered_map<std::string, VecSettings> MapSettings;
         VecSettings settings;
         MapSettings byCategory;
         //
         struct _CategoryToBeWritten { // state object used when saving INI settings
            _CategoryToBeWritten() {};
            _CategoryToBeWritten(std::string name, std::string header) : name(name), header(header) {};
            //
            std::string name;   // name of the category, used to look up INISetting pointers for found setting names
            std::string header; // the header line, including whitespace and comments
            std::string body;   // the body
            VecSettings found;  // found settings
            //
            void Write(INISettingManager* const, std::fstream&);
         };
         //
         VecSettings& GetCategoryContents(std::string category);
         //
      public:
         static INISettingManager& GetInstance();
         //
         void Add(INISetting* setting);
         void Load();
         void Save();
         //
         INISetting* Get(std::string& category, std::string& name) const;
         INISetting* Get(const char*  category, const char*  name) const;
         void ListCategories(std::vector<std::string>& out) const;
   };
   struct INISetting {
      enum ValueType {
         kType_Bool  = 0,
         kType_Float = 1,
         kType_SInt  = 2,
         kType_UInt  = 3,
      };
      INISetting(const char* n, const char* c, bool   b) : type(kType_Bool),  name(n), category(c), bDefault(b), bCurrent(b) { INISettingManager::GetInstance().Add(this); };
      INISetting(const char* n, const char* c, float  f) : type(kType_Float), name(n), category(c), fDefault(f), fCurrent(f) { INISettingManager::GetInstance().Add(this); };
      INISetting(const char* n, const char* c, SInt32 i) : type(kType_SInt),  name(n), category(c), iDefault(i), iCurrent(i) { INISettingManager::GetInstance().Add(this); };
      INISetting(const char* n, const char* c, UInt32 u) : type(kType_UInt),  name(n), category(c), uDefault(u), uCurrent(u) { INISettingManager::GetInstance().Add(this); };
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
      std::string ToString() const;
   };
};