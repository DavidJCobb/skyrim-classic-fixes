#include "skse/PluginAPI.h"		// super
#include "skse/skse_version.h"	// What version of SKSE is running?
#include "skse/SafeWrite.h"
#include "skse/GameAPI.h"
#include <shlobj.h>				// CSIDL_MYCODUMENTS
#include <psapi.h>  // MODULEINFO, GetModuleInformation
#pragma comment( lib, "psapi.lib" ) // needed for PSAPI to link properly
#include <string>

#include "Services/INI.h"
#include "Services/CrashLog.h"
#include "Patches/Exploratory.h"
#include "Patches/ArcheryDownwardArrowFix.h"
#include "Patches/ArmorAddonMO5SFix.h"
#include "Patches/UnderwaterAmbienceCellBoundaryFix.h"
#include "Patches/VampireFeedSoftlock.h"
#include "Patches/MerchantRestockFix.h"
#include "Patches/NPCTorchLandscapeFix.h"
#include "Patches/CrashFixes.h"
#include "Patches/ActiveEffectTimerBugs.h"
#include "Patches/ModArmorWeightPerk.h"

PluginHandle			       g_pluginHandle   = kPluginHandle_Invalid;
SKSEMessagingInterface*     g_ISKSEMessaging = nullptr;
SKSESerializationInterface* g_serialization  = nullptr;

static const char* g_pluginName = "CobbBugFixes";
const UInt32 g_pluginVersion   = 0x01050100; // 0xAABBCCDD = AA.BB.CC.DD with values converted to decimal // major.minor.update.internal-build-or-zero
const UInt32 g_serializationID = 'cBug';

void Callback_Messaging_SKSE(SKSEMessagingInterface::Message* message);
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
      SetupCrashLogging();
      CobbBugFixes::INISettingManager::GetInstance().Load();
      g_ISKSEMessaging->RegisterListener(g_pluginHandle, "SKSE", Callback_Messaging_SKSE);
      {  // Patches:
         CobbBugFixes::Patches::Exploratory::Apply();
         CobbBugFixes::Patches::ArcheryDownwardArrowFix::Apply();
         CobbBugFixes::Patches::ArmorAddonMO5SFix::Apply();
         CobbBugFixes::Patches::UnderwaterAmbienceCellBoundaryFix::Apply();
         CobbBugFixes::Patches::VampireFeedSoftlock::Apply();
         CobbBugFixes::Patches::NPCTorchLandscapeFix::Apply();
         CobbBugFixes::Patches::CrashFixes::Apply();
         CobbBugFixes::Patches::ActiveEffectTimerBugs::Apply();
         CobbBugFixes::Patches::ModArmorWeightPerk::Apply();
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
void Callback_Messaging_SKSE(SKSEMessagingInterface::Message* message) {
   if (message->type == SKSEMessagingInterface::kMessage_PostLoad) {
   } else if (message->type == SKSEMessagingInterface::kMessage_PostPostLoad) {
      SetupCrashLogging();
   } else if (message->type == SKSEMessagingInterface::kMessage_DataLoaded) {
   } else if (message->type == SKSEMessagingInterface::kMessage_NewGame) {
   } else if (message->type == SKSEMessagingInterface::kMessage_PreLoadGame) {
   } else if (message->type == SKSEMessagingInterface::kMessage_PostLoadGame) {
      //CobbBugFixes::Patches::Exploratory::ModelLoadingTest::RunTest();
   }
};
void Callback_Serialization_Save(SKSESerializationInterface* intfc) {
   _MESSAGE("Saving...");
   if (MerchantRestockFix::Save(intfc)) {
      _MESSAGE("Saving complete (or no data to save) for the merchant restock fix.");
   } else {
      _MESSAGE("Saving failed for the merchant restock fix.");
   }
   _MESSAGE("Saving done!");
}
void Callback_Serialization_Load(SKSESerializationInterface* intfc) {
   _MESSAGE("Loading...");
   //
   UInt32 type;
   UInt32 version;
   UInt32 length;
   bool   error = false;
   //
   while (!error && intfc->GetNextRecordInfo(&type, &version, &length)) {
      switch (type) {
         case MerchantRestockFix::ce_recordSignature:
            if (error = !MerchantRestockFix::Load(intfc, version))
               _MESSAGE("Loading failed for the merchant restock fix.");
            else
               _MESSAGE("Loading complete for the merchant restock fix.");
            break;
      }
   }
   //
   _MESSAGE("Loading done!");
}