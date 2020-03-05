#include "INI.h"
#include "skse/Utilities.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace CobbBugFixes {
   namespace INI {
      //
      // SETTING DEFINITIONS -- BEGIN
      //
      #define COBBBUGFIXES_MAKE_INI_SETTING(category, name, value) namespace category { extern INISetting name = INISetting(#name, #category, value); };
      //
      COBBBUGFIXES_MAKE_INI_SETTING(ActiveEffectTimerFixes, Enabled, true);
      COBBBUGFIXES_MAKE_INI_SETTING(CrashLogging, Enabled, false);
      COBBBUGFIXES_MAKE_INI_SETTING(MerchantRestockFixes, Enabled, true);
      COBBBUGFIXES_MAKE_INI_SETTING(NPCTorchLandscapeFix, Enabled, true);
      COBBBUGFIXES_MAKE_INI_SETTING(UnderwaterAmbienceCellBoundaryFix, Enabled, true);
      //
      COBBBUGFIXES_MAKE_INI_SETTING(CrashFixes, TESIdleFormDestructor, true);
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
      if (!c)
         return false;
      UInt8 i = 0;
      do {
         char d = ("FALSE")[i];
         if (c > 'Z') // is lowercase?
            d += 0x20; // to lowercase
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
   constexpr char c_iniComment = ';';
   constexpr char c_iniCategoryStart = '[';
   constexpr char c_iniCategoryEnd = ']';
   constexpr char c_iniKeyValueDelim = '=';
   //
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
   std::string INISetting::ToString() const {
      std::string out;
      this->ToString(out);
      return out;
   }
   void INISettingManager::_CategoryToBeWritten::Write(INISettingManager* const manager, std::fstream& file) {
      if (this->name.empty())
         return;
      file.write(this->header.c_str(), this->header.size());
      {  // Write missing settings.
         try {
            const auto& list = manager->GetCategoryContents(this->name);
            for (auto it = list.begin(); it != list.end(); ++it) {
               const INISetting* const setting = *it;
               if (std::find(this->found.begin(), this->found.end(), setting) == this->found.end()) {
                  std::string line = setting->name;
                  line += '=';
                  line += setting->ToString();
                  line += "\n"; // MSVC appears to treat '\n' in a const char* as "\r\n", automatically, without asking, so don't add a '\r'
                  file.write(line.c_str(), line.size());
               }
            }
         } catch (std::out_of_range) {};
      }
      file.write(this->body.c_str(), this->body.size());
   };
   //
   INISettingManager& INISettingManager::GetInstance() {
      static INISettingManager instance;
      return instance;
   };
   void INISettingManager::ListCategories(std::vector<std::string>& out) const {
      out.reserve(this->byCategory.size());
      for (auto it = this->byCategory.begin(); it != this->byCategory.end(); ++it)
         out.push_back(it->first);
   };
   INISetting* INISettingManager::Get(const char* category, const char* name) const {
      if (!category || !name)
         return nullptr;
      for (auto it = this->settings.begin(); it != this->settings.end(); ++it) {
         auto& setting = *it;
         if (!_stricmp(setting->category, category) && !_stricmp(setting->name, name))
            return setting;
      }
      return nullptr;
   };
   INISetting* INISettingManager::Get(std::string& category, std::string& name) const {
      return this->Get(category.c_str(), name.c_str());
   };
   void INISettingManager::Add(INISetting* setting) {
      this->settings.push_back(setting);
      //
      std::string category = setting->category;
      std::transform(category.begin(), category.end(), category.begin(), ::tolower);
      this->byCategory[category].push_back(setting);
   };
   INISettingManager::VecSettings& INISettingManager::GetCategoryContents(std::string category) {
      std::transform(category.begin(), category.end(), category.begin(), ::tolower);
      return this->byCategory[category];
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
      std::string   path = GetPath();
      std::ifstream file;
      file.open(path);
      if (!file) {
         _MESSAGE("Unable to load CobbBugFixes's INI file for reading. Calling CobbBugFixes::Save to generate a default one.");
         this->Save(); // generate a new INI file.
         return;
      }
      std::string category = "";
      std::string key      = "";
      while (!file.bad() && !file.eof()) {
         char buffer[1024];
         file.getline(buffer, sizeof(buffer));
         buffer[1023] = '\0';
         //
         bool   foundAny   = false; // found anything that isn't whitespace?
         bool   isCategory = false;
         INISetting* current = nullptr;
         UInt32 i = 0;
         char   c = buffer[0];
         if (!c)
            continue;
         do {
            if (!foundAny) {
               if (c == ' ' || c == '\t')
                  continue;
               if (c == c_iniComment) // lines starting with semicolons are comments
                  break;
               foundAny = true;
               if (c == c_iniCategoryStart) {
                  isCategory = true;
                  category   = "";
                  continue;
               }
               key = "";
            }
            if (isCategory) {
               if (c == c_iniCategoryEnd)
                  break;
               category += c;
               continue;
            }
            //
            // handling for setting names:
            //
            if (!current) {
               if (c == c_iniKeyValueDelim) {
                  current = this->Get(category, key);
                  i++;
                  break;
               }
               key += c;
            }
         } while (++i < sizeof(buffer) && (c = buffer[i]));
         //
         // Code from here on out assumes that (i) will not be modified -- it will always refer 
         // either to the end of the line or to the first character after the '='.
         //
         if (current) { // handling for setting values
            {  // Allow comments on the same line as a setting (but make sure we change this if we add string settings!)
               UInt32 j = i;
               do {
                  if (buffer[j] == '\0')
                     break;
                  if (buffer[j] == ';') {
                     buffer[j] = '\0';
                     break;
                  }
               } while (++j < sizeof(buffer));
            }
            switch (current->type) {
               case INISetting::kType_Bool:
                  {
                     if (_StringHelpers::string_says_false(buffer + i)) {
                        current->bCurrent = false;
                        break;
                     }
                     //
                     // treat numbers that compute to zero as "false:"
                     //
                     float value;
                     bool  final = true;
                     if (_StringHelpers::string_to_float(buffer + i, value)) {
                        final = (value != 0.0);
                     }
                     current->bCurrent = final;
                  }
                  break;
               case INISetting::kType_Float:
                  {
                     float value;
                     if (_StringHelpers::string_to_float(buffer + i, value))
                        current->fCurrent = value;
                  }
                  break;
               case INISetting::kType_SInt:
                  {
                     SInt32 value;
                     if (_StringHelpers::string_to_int(buffer + i, value))
                        current->iCurrent = value;
                  }
                  break;
               case INISetting::kType_UInt:
                  {
                     UInt32 value;
                     if (_StringHelpers::string_to_int(buffer + i, value))
                        current->uCurrent = value;
                  }
                  break;
            }
         }
      }
      file.close();
   };
   __declspec(noinline) void INISettingManager::Save() {
      _MESSAGE("Writing to CobbBugFixes's INI file...");
      std::string iPath = GetPath();
      std::string oPath = GetWorkingPath();
      std::fstream oFile;
      std::fstream iFile;
      oFile.open(oPath, std::ios_base::out | std::ios_base::trunc);
      if (!oFile) {
         _MESSAGE("Unable to open working INI file for writing.");
         return;
      }
      //
      _CategoryToBeWritten current;
      std::vector<std::string> missingCategories;
      this->ListCategories(missingCategories);
      iFile.open(iPath, std::ios_base::in);
      if (iFile) { // original file is optional; we'll generate a new one after this if it's not present
         bool beforeCategories = true;
         while (!iFile.bad() && !iFile.eof()) {
            std::string line;
            getline(iFile, line);
            line += '\n'; // add the terminator, since it (but not '\r') will be omitted from getline's result.
            UInt32 i    = 0;
            UInt32 size = line.size();
            bool   foundAny   = false; // found any non-whitespace chars
            bool   isCategory = false;
            bool   isValue    = false;
            std::string token;
            do {
               char c = line[i];
               if (!c)
                  break;
               if (c == c_iniComment) { // comment
                  if (isValue) {
                     UInt32 j = i; // we want to preserve any whitespace that followed the original value
                     do {
                        char d = line[--j];
                        if (!isspace(d))
                           break;
                     } while (j);
                     current.body += (line.c_str() + j + 1); // at this point, j will include one non-whitespace character BEFORE the trailing whitespace
                     break;
                  }
                  current.body += line;
                  break;
               }
               if (c == '\r' || c == '\n') { // end of line; make sure we get the whole terminator (we need this to catch the case of a setting value with no trailing comment)
                  current.body += (line.c_str() + i);
                  break;
               }
               if (!foundAny) {
                  if (c == c_iniCategoryStart) {
                     isCategory = true;
                     beforeCategories = false;
                     continue;
                  }
                  if (isspace(c))
                     continue;
                  foundAny = true;
               }
               if (isCategory) {
                  if (c == c_iniCategoryEnd) {
                     current.Write(this, oFile);
                     current = _CategoryToBeWritten(token, line);
                     {
                        std::transform(token.begin(), token.end(), token.begin(), ::tolower);
                        missingCategories.erase(std::remove(missingCategories.begin(), missingCategories.end(), token), missingCategories.end());
                     }
                     break;
                  }
                  token += c;
               } else {
                  if (c == c_iniKeyValueDelim) {
                     current.body.append(line, 0, i + 1);
                     auto ptr = this->Get(current.name.c_str(), token.c_str());
                     if (!ptr) {
                        current.body.append(line, i);
                        break;
                     }
                     current.body += ptr->ToString();
                     current.found.push_back(ptr);
                     isValue = true;
                     continue;
                  }
                  token += c;
               }
            } while (++i < size);
            if (beforeCategories) { // content before all categories
               oFile.write(line.c_str(), size);
            }
         }
         iFile.close();
      }
      current.Write(this, oFile); // write the last category, if necessary
      {  // Write missing categories.
         auto& list = missingCategories;
         for (auto it = list.begin(); it != list.end(); ++it) {
            oFile.put('\n'); // MSVC treats std::fstream.put('\n') as .write("\r\n", 2) automatically, without asking, so don't put('\r')
            std::string cat = *it;
            cat += "]\n"; // MSVC appears to treat '\n' in a const char* as "\r\n", automatically, without asking, so don't add a '\r'
            cat.insert(cat.begin(), '[');
            current = _CategoryToBeWritten(*it, cat);
            current.Write(this, oFile);
         }
      }
      _MESSAGE("INI temporary file written.");
      oFile.close();
      bool success = ReplaceFile(GetPath().c_str(), GetWorkingPath().c_str(), GetBackupPath().c_str(), 0, 0, 0);
      if (!success) {
         //if (!PathFileExists(GetPath().c_str())) {
            success = MoveFileEx(GetWorkingPath().c_str(), GetPath().c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
         //}
      }
      if (!success) {
         _MESSAGE("Unable to commit INI changes.");
      } else
         _MESSAGE("INI saved.");
   };
}