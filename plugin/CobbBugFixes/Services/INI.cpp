#include "INI.h"
#include "skse/Utilities.h"
#include <cctype>
#include <string>

namespace CobbBugFixes {
   namespace INI {
      //
      // SETTING DEFINITIONS -- BEGIN
      //
      #define COBBBUGFIXES_MAKE_INI_SETTING(category, name, value) namespace category { extern INISetting name = INISetting(#name, #category, value); };
      //
      COBBBUGFIXES_MAKE_INI_SETTING(UnderwaterAmbienceCellBoundaryFix, Enabled, true);
      //
      #undef COBBBUGFIXES_MAKE_INI_SETTING
      //
      // SETTING DEFINITIONS -- END
      //
      // INTERNALS BELOW
      //
   };
};

const std::string& GetPath() {
   static std::string path;
   if (path.empty()) {
      std::string	runtimePath = GetRuntimeDirectory();
      if (!runtimePath.empty()) {
         path = runtimePath;
         path += "Data\\SKSE\\Plugins\\CobbBugFixes.ini";
      }
   }
   return path;
};
const std::string& GetWorkingPath() {
   static std::string path;
   if (path.empty()) {
      std::string	runtimePath = GetRuntimeDirectory();
      if (!runtimePath.empty()) {
         path = runtimePath;
         path += "Data\\SKSE\\Plugins\\CobbBugFixes.ini.tmp";
      }
   }
   return path;
};
const std::string& GetBackupPath() {
   static std::string path;
   if (path.empty()) {
      std::string	runtimePath = GetRuntimeDirectory();
      if (!runtimePath.empty()) {
         path = runtimePath;
         path += "Data\\SKSE\\Plugins\\CobbBugFixes.ini.bak";
      }
   }
   return path;
};

namespace _StringHelpers {
   UInt32 _baseOfInt(const char* str) {
      const char* p = str;
      while (char c = *p) {
         if (isspace(c))
            continue;
         if (c == '0') {
            ++p;
            c = *p;
            if (c == 'x' || c == 'X')
               return 16;
         }
         ++p;
      }
      return 10;
   };
   bool _hasNonWhitespace(const char* str) {
      char c = *str;
      while (c) {
         if (!isspace(c))
            return true;
         ++str;
         c = *str;
      }
      return false;
   };
   //
   bool string_says_false(const char* str) {
      char c = *str;
      while (c) { // skip leading whitespace
         if (!std::isspace(c))
            break;
         str++;
         c = *str;
      }
      if (c) {
         UInt8 i = 0;
         do {
            char d = ("FALSEfalse")[i + (c > 'Z' ? 5 : 0)];
            if (c != d)
               return false;
            str++;
            c = *str;
         } while (++i < 5);
         //
         // Ignoring leading whitespace, the string starts with "FALSE". Return true if no non-whitespace 
         // chars follow that.
         //
         while (c) {
            if (!std::isspace(c))
               return false;
            str++;
            c = *str;
         }
         return true;
      }
      return false;
   };
   bool string_to_float(const char* str, float& out) {
      errno = 0;
      char* end = nullptr;
      float o = strtof(str, &end);
      if (end == str) // not a number
         return false;
      if (_hasNonWhitespace(end)) // if any non-whitespace chars after the found value, then the string isn't really a number
         return false;
      if (errno == ERANGE) // out of range
         return false;
      out = o;
      return true;
   };
   bool string_to_int(const char* str, SInt32& out, bool allowHexOrDecimal = false) {
      errno = 0;
      char* end = nullptr;
      UInt32 base = 10;
      if (allowHexOrDecimal)
         base = _baseOfInt(str);
      SInt32 o = strtol(str, &end, base);
      if (end == str) // not a number
         return false;
      if (_hasNonWhitespace(end)) // if any non-whitespace chars after the found value, then the string isn't really a number
         return false;
      if (errno == ERANGE) // out of range
         return false;
      out = o;
      return true;
   };
   bool string_to_int(const char* str, UInt32& out, bool allowHexOrDecimal = false) {
      errno = 0;
      char* end = nullptr;
      UInt32 base = 10;
      if (allowHexOrDecimal)
         base = _baseOfInt(str);
      UInt32 o = strtoul(str, &end, base);
      if (end == str) // not a number
         return false;
      if (_hasNonWhitespace(end)) // if any non-whitespace chars after the found value, then the string isn't really a number
         return false;
      if (errno == ERANGE) // out of range
         return false;
      out = o;
      return true;
   };
}

constexpr char c_iniComment = ';';
bool _getIniSetting(const char* section, const char* key, const std::string& path, std::string& out) {
   char resultBuf[256];
   resultBuf[0] = 0;
   UInt32 resultLen = GetPrivateProfileString(section, key, NULL, resultBuf, sizeof(resultBuf), path.c_str());
   out.assign(resultBuf, resultLen);
   out = out.substr(0, out.find(c_iniComment));
   //_MESSAGE("Loaded INI setting: [%s]%s=%s", section, key, out.c_str());
   return true;
};

namespace CobbBugFixes {
   void INISetting::ToString(std::string& out) const {
      char working[20];
      switch (this->type) {
         case INISetting::kType_Bool:
            out = this->bCurrent ? "TRUE" : "FALSE";
            break;
         case INISetting::kType_Float:
            sprintf_s(working, "%f", this->fCurrent);
            working[19] = '\0';
            out = working;
            break;
         case INISetting::kType_SInt:
            sprintf_s(working, "%i", this->iCurrent);
            working[19] = '\0';
            out = working;
            break;
         case INISetting::kType_UInt:
            sprintf_s(working, "%u", this->iCurrent);
            working[19] = '\0';
            out = working;
            break;
         default:
            out.clear();
      }
   };
   //
   INISettingManager& INISettingManager::GetInstance() {
      static INISettingManager instance;
      return instance;
   };
   //
   void _saveIniSetting(INISetting& setting, const std::string& path) {
      std::string line;
      setting.ToString(line);
      WritePrivateProfileStringA(setting.category, setting.name, line.c_str(), path.c_str());
   }
   void _loadIniSetting(INISetting& setting, const std::string& path) {
      std::string buffer;
      _getIniSetting(setting.category, setting.name, path, buffer);
      switch (setting.type) {
         case INISetting::kType_Bool:
            {
               auto buf = buffer.c_str();
               if (_StringHelpers::string_says_false(buf)) {
                  setting.bCurrent = false;
                  break;
               }
               //
               // treat numbers that compute to zero as "false:"
               //
               float value;
               bool  final = true;
               if (_StringHelpers::string_to_float(buf, value)) {
                  final = (value != 0.0);
               }
               setting.bCurrent = final;
            }
            break;
         case INISetting::kType_Float:
            {
               auto buf = buffer.c_str();
               float value;
               if (_StringHelpers::string_to_float(buf, value))
                  setting.fCurrent = value;
            }
            break;
         case INISetting::kType_SInt:
            {
               auto buf = buffer.c_str();
               SInt32 value;
               if (_StringHelpers::string_to_int(buf, value))
                  setting.iCurrent = value;
            }
            break;
         case INISetting::kType_UInt:
            {
               auto buf = buffer.c_str();
               UInt32 value;
               if (_StringHelpers::string_to_int(buf, value))
                  setting.iCurrent = value;
            }
            break;
      }
   };
   __declspec(noinline) void INISettingManager::Load() {
      std::string path = GetPath();
      if (path.empty()) {
         return;
      }
      _loadIniSetting(INI::UnderwaterAmbienceCellBoundaryFix::Enabled, path);
   };
   __declspec(noinline) void INISettingManager::Save() {
      std::string path = GetPath();
      if (path.empty()) {
         return;
      }
      _saveIniSetting(INI::UnderwaterAmbienceCellBoundaryFix::Enabled, path);
   };
}