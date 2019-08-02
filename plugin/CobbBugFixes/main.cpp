#include "skse/PluginAPI.h"		// super
#include "skse/skse_version.h"	// What version of SKSE is running?
#include "skse/SafeWrite.h"
#include "skse/GameAPI.h"
#include <shlobj.h>				// CSIDL_MYCODUMENTS
#include <psapi.h>  // MODULEINFO, GetModuleInformation
#pragma comment( lib, "psapi.lib" ) // needed for PSAPI to link properly
#include <string>

#include "Services/INI.h"
#include "Patches/Exploratory.h"
#include "Patches/ArcheryDownwardArrowFix.h"
#include "Patches/ArmorAddonMO5SFix.h"
#include "Patches/UnderwaterAmbienceCellBoundaryFix.h"
#include "Patches/VampireFeedSoftlock.h"
#include "Patches/MerchantRestockFix.h"

PluginHandle			       g_pluginHandle   = kPluginHandle_Invalid;
SKSEMessagingInterface*     g_ISKSEMessaging = nullptr;
SKSESerializationInterface* g_serialization  = nullptr;

static const char* g_pluginName = "CobbBugFixes";
const UInt32 g_pluginVersion   = 0x01020000; // 0xAABBCCDD = AA.BB.CC.DD with values converted to decimal // major.minor.update.internal-build-or-zero
const UInt32 g_serializationID = 'cBug';

void Callback_Serialization_Save(SKSESerializationInterface* intfc);
void Callback_Serialization_Load(SKSESerializationInterface* intfc);

extern "C" {
   //
   // SKSEPlugin_Query: Called by SKSE to learn about this plug-in and check that it's safe to load.
   //
   bool SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info) {
      gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim\\SKSE\\CobbBugFixes.log");
      gLog.SetPrintLevel(IDebugLog::kLevel_Error);
      gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);
      //
      _MESSAGE("Query");
      //
      // Populate info structure.
      //
      info->infoVersion = PluginInfo::kInfoVersion;
      info->name        = g_pluginName;
      info->version     = g_pluginVersion;
      {  // Log version number
         auto  v     = info->version;
         UInt8 main  = v >> 0x18;
         UInt8 major = v >> 0x10;
         UInt8 minor = v >> 0x08;
         _MESSAGE("Current version is %d.%d.%d.", main, major, minor);
      }
      {  // Get run-time information
         HMODULE    baseAddr = GetModuleHandle("CobbBugFixes"); // DLL filename
         MODULEINFO info;
         if (baseAddr && GetModuleInformation(GetCurrentProcess(), baseAddr, &info, sizeof(info)))
            _MESSAGE("We're loaded to the span of memory at %08X - %08X.", info.lpBaseOfDll, (UInt32)info.lpBaseOfDll + info.SizeOfImage);
      }
      //
      // Store plugin handle so we can identify ourselves later.
      //
      g_pluginHandle = skse->GetPluginHandle();
      //
      //g_SKSEVersionSupported = (skse->skseVersion >= 0x01070300); // 1.7.3.0
      //
      if (skse->isEditor) {
         _MESSAGE("We've been loaded in the Creation Kit. Marking as incompatible.");
         return false;
      } else if (skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0) {
         _MESSAGE("Unsupported Skyrim version: %08X.", skse->runtimeVersion);
         return false;
      }
      {  // Get the messaging interface and query its version.
         g_ISKSEMessaging = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
         if (!g_ISKSEMessaging) {
            _MESSAGE("Couldn't get messaging interface.");
            return false;
         } else if (g_ISKSEMessaging->interfaceVersion < SKSEMessagingInterface::kInterfaceVersion) {
            _MESSAGE("Messaging interface too old (%d; we expected %d).", g_ISKSEMessaging->interfaceVersion, SKSEMessagingInterface::kInterfaceVersion);
            return false;
         }
      }
      {  // Get the serialization interface and query its version.
         g_serialization = (SKSESerializationInterface*)skse->QueryInterface(kInterface_Serialization);
         if (!g_serialization) {
            _MESSAGE("Couldn't get serialization interface.");
            return false;
         } else if (g_serialization->version < SKSESerializationInterface::kVersion) {
            _MESSAGE("Serialization interface too old (%d; we expected %d).", g_serialization->version, SKSESerializationInterface::kVersion);
            return false;
         }
      }
      //
      // This plug-in supports the current Skyrim and SKSE versions:
      //
      return true;
   }
   //
   // SKSEPlugin_Load: Called by SKSE to load this plug-in.
   //
   bool SKSEPlugin_Load(const SKSEInterface* skse) {
      _MESSAGE("Load.");
      CobbBugFixes::INISettingManager::GetInstance().Load();
      {  // Patches:
         CobbBugFixes::Patches::Exploratory::Apply();
         CobbBugFixes::Patches::ArcheryDownwardArrowFix::Apply();
         CobbBugFixes::Patches::ArmorAddonMO5SFix::Apply();
         CobbBugFixes::Patches::UnderwaterAmbienceCellBoundaryFix::Apply();
         CobbBugFixes::Patches::VampireFeedSoftlock::Apply();
         MerchantRestockFixManager::applyHook();
      }
      {  // Serialization
         g_serialization->SetUniqueID(g_pluginHandle, g_serializationID);
         //g_serialization->SetRevertCallback(g_pluginHandle, Callback_Serialization_Revert);
         g_serialization->SetSaveCallback  (g_pluginHandle, Callback_Serialization_Save);
         g_serialization->SetLoadCallback  (g_pluginHandle, Callback_Serialization_Load);
      }
      return true;
   }
};
void Callback_Serialization_Save(SKSESerializationInterface* intfc) {
   _MESSAGE("Saving...");
   if (MerchantRestockFixManager::GetInstance().Save(intfc)) {
      _MESSAGE("MerchantRestockFixManager saved successfully or had no data to save.");
   } else {
      _MESSAGE("MerchantRestockFixManager failed to save.");
   }
   _MESSAGE("Saving done!");
}
void Callback_Serialization_Load(SKSESerializationInterface* intfc) {
   _MESSAGE("Loading...");
   //
   UInt32 type;    // This IS correct. A UInt32 and a four-character ASCII string have the same length (and can be read interchangeably, it seems).
   UInt32 version;
   UInt32 length;
   bool   error = false;
   //
   while (!error && intfc->GetNextRecordInfo(&type, &version, &length)) {
      switch (type) {
         case MerchantRestockFixManager::recordSignature:
            if (error = !MerchantRestockFixManager::GetInstance().Load(intfc, version))
               _MESSAGE("MerchantRestockFixManager failed to load.");
            else
               _MESSAGE("MerchantRestockFixManager loaded.");
            break;
      }
   }
   //
   _MESSAGE("Loading done!");
}