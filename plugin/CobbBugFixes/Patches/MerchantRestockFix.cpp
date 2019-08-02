#include "MerchantRestockFix.h"
#include "ReverseEngineered\Forms\TESFaction.h"
#include "ReverseEngineered\Forms\TESObjectREFR.h"
#include "ReverseEngineered\Systems\Timing.h"
#include "ReverseEngineered\GameSettings.h"
//
#include "skse/SafeWrite.h"
#include "skse/Serialization.h"
#include "skse/GameRTTI.h"

namespace Hook {
   void _stdcall Inner(RE::TESFaction* faction) {
      MerchantRestockFixManager::GetInstance().doRestockHandling(faction);
   }
   __declspec(naked) void Outer() {
      _asm {
         push ecx;
         call Inner; // stdcall
         retn;
      }
   }
}
void MerchantRestockFixManager::applyHook() {
   WriteRelJump(0x0055CF60, (UInt32)Hook::Outer); // overwrite TESFaction::DoMerchantRestockCheck
}
SInt32 MerchantRestockFixManager::getLastVisit(RE::TESFaction* faction) {
   std::lock_guard<decltype(this->lock)> guard(this->lock);
   //
   auto& vendor = faction->vendorData;
   auto& map    = this->lastVisit;
   if (vendor.lastShoppedAt == -1) {
      auto it = map.find(faction->formID);
      if (it != map.end())
         vendor.lastShoppedAt = it->second;
   } else {
      this->lastVisit[faction->formID] = vendor.lastShoppedAt;
   }
   return vendor.lastShoppedAt;
}
void MerchantRestockFixManager::setLastVisit(RE::TESFaction* faction, SInt32 days) {
   std::lock_guard<decltype(this->lock)> guard(this->lock);
   //
   faction->vendorData.lastShoppedAt = days;
   this->lastVisit[faction->formID] = days;
}
void MerchantRestockFixManager::doRestockHandling(RE::TESFaction* faction) {
   std::lock_guard<decltype(this->lock)> guard(this->lock);
   //
   auto container = (RE::TESObjectREFR*) faction->vendorData.merchantContainer;
   if (!container)
      return;
   auto& manager = MerchantRestockFixManager::GetInstance();
   auto  today = CALL_MEMBER_FN(*RE::g_timeGlobals, GetGameDaysPassed)();
   auto  last = manager.getLastVisit(faction);
   if (last == -1)
      return;
   if ((today - last) > RE::GMST::iDaysToRespawnVendor->data.s32)
      return;
   manager.setLastVisit(faction, today);
   container->ResetInventory(!CALL_MEMBER_FN(container, DoesRespawn)());
}

bool MerchantRestockFixManager::Save(SKSESerializationInterface* intfc) {
   using namespace Serialization;
   std::lock_guard<decltype(this->lock)> guard(this->lock);
   //
   UInt32 count = this->lastVisit.size();
   if (!count)
      return true;
   if (intfc->OpenRecord(MerchantRestockFixManager::recordSignature, kSaveVersion)) {
      intfc->WriteRecordData(&count, sizeof(count));
      for (auto it = this->lastVisit.begin(); it != this->lastVisit.end(); ++it) {
         FormID id   = it->first;
         SInt32 days = it->second;
         WriteData(intfc, &id);
         WriteData(intfc, &days);
      }
   }
   return true;
}
bool MerchantRestockFixManager::Load(SKSESerializationInterface* intfc, UInt32 version) {
   using namespace Serialization;
   std::lock_guard<decltype(this->lock)> guard(this->lock);
   //
   this->lastVisit.clear();
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
      this->lastVisit[id] = days;
   }
   return true;
}
void MerchantRestockFixManager::Revert() {
   std::lock_guard<decltype(this->lock)> guard(this->lock);
   this->lastVisit.clear();
}