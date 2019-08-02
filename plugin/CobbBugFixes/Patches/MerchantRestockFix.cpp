#include "MerchantRestockFix.h"
#include "ReverseEngineered\Forms\TESFaction.h"
#include "ReverseEngineered\Systems/GameData.h"
//
#include "skse/SafeWrite.h"
#include "skse/Serialization.h"
#include "skse/GameRTTI.h"

#include <vector>

struct _TempEntry {
   UInt32 formID;
   UInt32 days;
   //
   _TempEntry(UInt32 a, UInt32 b) : formID(a), days(b) {}
};

bool MerchantRestockFix::Save(SKSESerializationInterface* intfc) {
   using namespace Serialization;
   //
   auto  dh   = RE::DataHandler::GetSingleton();
   auto& list = dh->factions;
   //
   std::vector<_TempEntry> map;
   //
   for (UInt32 i = 0; i < list.count; i++) {
      auto faction = (RE::TESFaction*) list.arr.entries[i];
      if (faction && faction->vendorData.merchantContainer) {
         auto days = faction->vendorData.lastReset;
         if (days != 0xFFFFFFFF)
            map.emplace_back(faction->formID, days);
      }
   }
   auto count = map.size();
   if (!count)
      return true;
   if (intfc->OpenRecord(ce_recordSignature, kSaveVersion)) {
      intfc->WriteRecordData(&count, sizeof(count));
      for (auto it = map.begin(); it != map.end(); ++it) {
         FormID id   = (*it).formID;
         UInt32 days = (*it).days;
         WriteData(intfc, &id);
         WriteData(intfc, &days);
      }
   }
   return true;
}
bool MerchantRestockFix::Load(SKSESerializationInterface* intfc, UInt32 version) {
   using namespace Serialization;
   //
   UInt32 count = 0;
   if (!ReadData(intfc, &count)) {
      _MESSAGE(__FUNCTION__ ": Failed to read the faction count.");
      return false;
   }
   for (UInt32 i = 0; i < count; i++) {
      FormID id;
      SInt32 days;
      if (!ReadData(intfc, &id) || !ReadData(intfc, &days)) {
         _MESSAGE(__FUNCTION__ ": Failed to read record %i.", i);
         return false;
      }
      if (!intfc->ResolveFormId(id, &id)) {
         _MESSAGE(__FUNCTION__ ": Skipping form ID %08X; the mod that defined this faction appears to have been removed.");
         continue;
      }
      auto faction = (RE::TESFaction*) DYNAMIC_CAST(LookupFormByID(id), TESForm, TESFaction);
      if (!faction) {
         _MESSAGE(__FUNCTION__ ": Skipping form ID %08X; the mod that defined this faction appears to have changed, and the form ID is now being used by something else.");
         continue;
      }
      faction->vendorData.lastReset = days;
   }
   return true;
}