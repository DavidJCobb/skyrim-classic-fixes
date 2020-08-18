#include "CrashLog.h"
#include "CrashLogDefinitions.h"
#include "Patches/DetectShutdown.h"
#include <algorithm> // std::min
#include <cstdint>
#include <psapi.h>  // MODULEINFO, GetModuleInformation
#pragma comment( lib, "psapi.lib" ) // needed for PSAPI to link properly
#include <string>
#include <vector>
#include "helpers/rtti.h"
#include "helpers/strings.h"
#include "skse/GameAPI.h"

#include "INI.h"

constexpr uint32_t ce_printStackCount = 40;

using tstring = std::basic_string<TCHAR>;

struct _module {
   tstring  name;
   tstring  file;
   uint32_t base = 0;
   uint32_t end  = 0;
   //
   _module() {}
   _module(tstring&& s, uint32_t b, uint32_t e) : name(s), base(b), end(e) {
      if (!this->name.empty()) {
         auto i = this->name.find_last_of("\\/");
         if (i == std::string::npos) {
            this->file = this->name;
         } else {
            this->file = this->name.substr(i + 1);
         }
      }
   }
   //
   inline bool contains_address(uint32_t a) const noexcept { return a >= this->base && a < this->end; }
   inline bool operator<(const _module& other) const noexcept { return this->base < other.base; }
   inline bool operator>(const _module& other) const noexcept { return this->base > other.base; }
   //
   inline bool is_base_game() const noexcept { return this->file == "TESV.exe"; }
};
using module_list_t = std::vector<_module>;

namespace {
   const CrashLogLabel* GetLabel(UInt32 addr) {
      for (UInt32 i = 0; i < g_labelCount; i++) {
         auto&  entry = g_labels[i];
         if (addr >= entry.start && addr <= (entry.start + entry.size))
            return &entry;
      }
      return nullptr;
   }
}

static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI s_originalFilter = nullptr;

void _load_modules(module_list_t& list) {
   constexpr int LOAD_COUNT = 140;
   //
   list.clear();
   //
   HANDLE  processHandle = GetCurrentProcess();
   HMODULE modules[LOAD_COUNT];
   DWORD   bytesNeeded;
   bool    success = EnumProcessModules(processHandle, modules, sizeof(modules), &bytesNeeded);
   bool    overflow = success && (bytesNeeded > (LOAD_COUNT * sizeof(HMODULE)));
   if (!success)
      return;
   //
   uint32_t   count = (std::min)(bytesNeeded / sizeof(HMODULE), (UInt32)LOAD_COUNT);
   MODULEINFO moduleData;
   for (uint32_t i = 0; i < count; i++) {
      if (GetModuleInformation(processHandle, modules[i], &moduleData, sizeof(moduleData))) {
         uint32_t base = (uint32_t)moduleData.lpBaseOfDll;
         uint32_t end = base + moduleData.SizeOfImage;
         //
         tstring name;
         name.resize(MAX_PATH);
         while (!GetModuleFileNameEx(processHandle, modules[i], name.data(), name.size())) {
            name.clear();
            if (name.size() > 4096)
               break;
            name.resize(name.size() + MAX_PATH);
         }
         //
         list.emplace_back(std::move(name), base, end);
      }
   }
   std::sort(list.begin(), list.end());
}

void _print_register(const char* name, uint32_t value, const module_list_t& modules) {
   for (auto& module : modules) {
      if (module.contains_address(value)) {
         _MESSAGE("%s | %08X (%s+%05X)", name, value, module.file.c_str(), value - module.base);
         return;
      }
   }
   _MESSAGE("%s | %08X", name, value);
}

const char* _try_get_class_name(uint32_t address) {
   constexpr int max_mangled_name_length = 2048;
   //
   if (address < 4)
      return nullptr;
   __try {
      if (IsBadReadPtr((const void*)address, sizeof(void*)))
         return nullptr;
      uint32_t vtbl = *(uint32_t*)address;
      if (IsBadReadPtr((const void*)(vtbl - 4), sizeof(void*)))
         return nullptr;
      cobb::rtti::complete_object_locator* rtti = (cobb::rtti::complete_object_locator*) *(uint32_t*)(vtbl - 4);
      if (IsBadReadPtr(rtti, sizeof(cobb::rtti::complete_object_locator)))
         return nullptr;
      auto typeinfo = rtti->typeinfo;
      if (IsBadReadPtr(typeinfo, sizeof(cobb::rtti::type_descriptor)))
         return nullptr;
      auto name = typeinfo->name();
      if (IsBadStringPtrA(name, max_mangled_name_length))
         return nullptr;
      uint32_t i;
      for (i = 0; i < max_mangled_name_length; ++i) {
         char c = name[i];
         if (!c)
            break;
         if (c == '\r' || c == '\n' || c == '\t' || !isprint(c)) // Skyrim RTTI should only have printable characters
            i = max_mangled_name_length;
      }
      if (i >= max_mangled_name_length)
         return nullptr;
      return typeinfo->name();
   } __except (EXCEPTION_EXECUTE_HANDLER) {}
   return nullptr;
}
void _print_stack(uint32_t offset, uint32_t value, const module_list_t& modules) {
   std::string out;
   cobb::sprintf(out, "[ESP+%04X] %08X | ", offset, value);
   auto name = _try_get_class_name(value);
   if (name) {
      out += "instance of ";
      out += name;
   }
   for (auto& module : modules) {
      if (module.contains_address(value)) {
         if (name)
            out += " | ";
         std::string temp;
         cobb::sprintf(temp, "%s+%05X", module.file.c_str(), value - module.base);
         out += temp;
         break;
      }
   }
   _MESSAGE(out.c_str());
}

bool _instruction_is_lock_add(const char* addr) {
   if (IsBadReadPtr(addr, 2))
      return false;
   uint8_t b0 = *(uint8_t*)(addr + 0);
   if (b0 == 0xF0) {
      ++addr;
      b0 = *(uint8_t*)(addr + 0);
      if (IsBadReadPtr(addr + 1, 1))
         return false;
   }
   if (b0 != 0x0F)
      return false;
   uint8_t b1 = *(uint8_t*)(addr + 1);
   if (b1 != 0xC0 && b1 != 0xC1)
      return false;
   return true;
}
bool _is_smart_pointer_crash(PCONTEXT cr) {
   if (!CobbBugFixes::Patches::DetectShutdown::is_shutting_down)
      return false;
   uint32_t eip = cr->Eip;
   if (_instruction_is_lock_add((const char*)eip)) { // check for LOCK XADD by -1
      uint8_t  first = *(uint8_t*)eip;
      uint8_t* arg   = (uint8_t*) ((first == 0x0F) ? eip + 2 : eip + 3);
      if (!IsBadReadPtr(arg, 1)) {
         auto    b   = *arg;
         uint8_t mod = b >> 6;         // indicates whether it's [target + something] and if so, the bytecount of (something)
         uint8_t reg = b >> 3 & 0b111; // source
         uint8_t r_m = b & 0b111;      // target
         //
         uint32_t operand = 0;
         switch (reg) {
            case 0b000: operand = cr->Eax; break;
            case 0b001: operand = cr->Ecx; break;
            case 0b010: operand = cr->Edx; break;
            case 0b011: operand = cr->Ebx; break;
            case 0b100: operand = cr->Esp; break; // should never happen
            case 0b101: operand = cr->Ebp; break;
            case 0b110: operand = cr->Esi; break;
            case 0b111: operand = cr->Edi; break;
         }
         if (operand == 0xFFFFFFFF)
            return true;
      }
   }
   return false;
}

void _logCrash(EXCEPTION_POINTERS* info) {
   _MESSAGE("\n\nUnhandled exception (i.e. crash) caught!");
   std::vector<_module> modules;
   _load_modules(modules);
   //
   uint32_t eip = info->ContextRecord->Eip;
   {
      auto label = GetLabel(eip);
      if (label) {
         if (label->type != CrashLogLabel::Type::subroutine) {
            _MESSAGE("Instruction pointer (EIP): %08X (not-a-subroutine:%s)", eip, label->name);
         } else {
            _MESSAGE("Instruction pointer (EIP): %08X (%s+%02X)", eip, label->name, (eip - label->start));
         }
      } else {
         _MESSAGE("Instruction pointer (EIP): %08X", eip);
      }
   }
   _MESSAGE("\nREG | VALUE");
   _print_register("eax", info->ContextRecord->Eax, modules);
   _print_register("ebx", info->ContextRecord->Ebx, modules);
   _print_register("ecx", info->ContextRecord->Ecx, modules);
   _print_register("edx", info->ContextRecord->Edx, modules);
   _print_register("edi", info->ContextRecord->Edi, modules);
   _print_register("esi", info->ContextRecord->Esi, modules);
   _print_register("ebp", info->ContextRecord->Ebp, modules);
   {  // Print stack
      _MESSAGE("\nSTACK (esp == %08X):", info->ContextRecord->Esp);
      uint32_t* esp = (uint32_t*)info->ContextRecord->Esp;
      uint32_t i = 0;
      do {
         auto p = esp[i];
         _print_stack(i * 4, p, modules);
      } while (++i < CobbBugFixes::INI::CrashLogging::StackCount.uCurrent);
   }
   _MESSAGE("\n");
   {  // Module debug.
      if (modules.size()) {
         bool found = false;
         for (auto& module : modules) {
            if (module.contains_address(eip)) {
               found = true;
               if (module.file != "TESV.exe" && _is_smart_pointer_crash(info->ContextRecord)) {
                  _MESSAGE("GAME CRASHED AT INSTRUCTION Base+0x%08X IN MODULE: %s", (eip - module.base), module.name.c_str());
                  _MESSAGE("This appears to be a harmless smart pointer crash-on-exit, possibly caused by an \n"
                           "SKSE DLL. Allow me to explain:\n\n"
                           "One of the challenges that programmers have to deal with is memory management: we \n"
                           "need to make sure that when we're done using some piece of data, we delete it and \n"
                           "free up the memory it's using. The trick is to make sure that we don't forget to \n"
                           "delete objects (a memory leak), or accidentally delete objects early (a crash), or \n"
                           "accidentally delete something twice (a very hard-to-fix crash). One of the tricks \n"
                           "that we have to deal with this challenge is a \"smart pointer.\" The basic concept is, \n"
                           "we give the piece of data a \"reference count\" which indicates how many parts of our \n"
                           "program are using the data, and we only ever refer to the data through \"smart \n"
                           "pointers\" that automatically manage this reference count. When we create a smart \n"
                           "pointer, it automatically increases the reference count. When we're done with a \n"
                           "smart pointer and we throw it away, it automatically decreases the reference \n"
                           "count... and if the reference count then hits zero, the smart pointer deletes the \n"
                           "data. The smart pointer essentially says, \"If I die, and also all my friends are \n"
                           "dead, then I'm takin' you with me!\"\n\n"
                           "When SKSE DLLs try to use smart pointers to refer to Skyrim's game data, however, \n"
                           "they can get tripped up when the game closes down. When Skyrim closes down, it \n"
                           "doesn't painstakingly go through every single piece of data it's been working with \n"
                           "to delete them one by one. Instead, it just takes the entire \"heap\" -- the entire \n"
                           "memory space where game data is stored -- and deletes it all at once, and \"delete\" \n"
                           "in this context means that Skyrim tells Windows, \"Hey, I'm done with this. It can \n"
                           "be used for something else.\"\n\n"
                           "Here's what happens when an SKSE DLL uses a smart pointer to refer to something on \n"
                           "that heap. The SKSE DLL shuts down after Skyrim has thrown the heap away. The DLL's \n"
                           "smart pointer doesn't know that the data it's keeping track of is gone, so it'll \n"
                           "dutifully try to decrease the reference count; and since Skyrim has already told \n"
                           "Windows that the memory at that location is no longer in use, Windows goes, \"Hold \n"
                           "on, you're not supposed to be touching that,\" and a crash occurs.\n\n"
                           "Since the crash is occurring after the game has almost fully shut down, it shouldn't \n"
                           "interfere with gameplay, your savedata, etc.. It's a flaw in whatever DLL is causing \n"
                           "it, but it's a harmless flaw. The fix would be for the SKSE DLL's author to add \n"
                           "shutdown code, to painstakingly go through all of its smart pointers and safely \n"
                           "clear them (i.e. throw them away without decreasing any reference counts).");
               } else {
                  _MESSAGE("GAME CRASHED AT INSTRUCTION Base+0x%08X IN MODULE: %s", (eip - module.base), module.name.c_str());
                  _MESSAGE("Please note that this does not automatically mean that that module is responsible. \n"
                           "It may have been supplied bad data or program state as the result of an issue in \n"
                           "the base game or a different DLL.");
               }
               break;
            }
         }
         if (!found) {
            _MESSAGE("UNABLE TO IDENTIFY MODULE CONTAINING THE CRASH ADDRESS.");
            _MESSAGE("This can occur if the crashing instruction is located in the vanilla address space, \n"
                     "but it can also occur if there are too many DLLs for us to list, and if the crash \n"
                     "occurred in one of their address spaces. Please note that even if the crash occurred \n"
                     "in vanilla code, that does not necessarily mean that it is a vanilla problem. The \n"
                     "vanilla code may have been supplied bad data or program state as the result of an \n"
                     "issue in a loaded DLL.");
         }
         _MESSAGE("\nLISTING MODULE BASES...");
         for (auto& module : modules) {
            _MESSAGE(" - 0x%08X - 0x%08X: %s", module.base, module.end, module.name.c_str());
         }
         _MESSAGE("END OF LIST.");
      } else {
         _MESSAGE("UNABLE TO EXAMINE LOADED DLLs.");
      }
   }
   _MESSAGE("\nALL DATA PRINTED.");
}
LONG WINAPI _filter(EXCEPTION_POINTERS* info) {
   static bool caught = false;
   bool ignored = false;
   //
   if (!caught) {
      caught = true;
      try {
         _logCrash(info);
      } catch (...) {};
   } else
      ignored = true;
   if (s_originalFilter)
      s_originalFilter(info); // don't return
   return !ignored ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER;
};

void SetupCrashLogging() {
   if (CobbBugFixes::INI::CrashLogging::Enabled.bCurrent == false) {
      return;
   }
   auto f = SetUnhandledExceptionFilter(&_filter);
   if (f != &_filter) {
      s_originalFilter = f;
      _MESSAGE("Applied our unhandled exception filter; if it's not clobbered, then we'll be ready to catch crashes.");
   }
}