#pragma once
#include <map>
#include <mutex>
#include "skse/PluginAPI.h"

namespace RE {
   class TESFaction;
}
class MerchantRestockFixManager {
   public:
      typedef UInt32 FormID;
   protected:
      std::map<FormID, SInt32> lastVisit;
      mutable std::recursive_mutex lock;
   public:
      static MerchantRestockFixManager& GetInstance() {
         static MerchantRestockFixManager instance;
         return instance;
      };
      static void applyHook();
      //
      SInt32 getLastVisit(RE::TESFaction*);
      void   setLastVisit(RE::TESFaction*, SInt32 days);
      //
      void doRestockHandling(RE::TESFaction*);
      //
      enum { kSaveVersion = 1 };
      static constexpr UInt32 recordSignature = 'Mrch';
      bool Save(SKSESerializationInterface* intfc);
      bool Load(SKSESerializationInterface* intfc, UInt32 version);
      void Revert();
};

namespace CobbBugFixes {
   namespace Patches {
      namespace MerchantRestockFix {
         void Apply();
      }
   }
}