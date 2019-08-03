#pragma once
#include <map>
#include <mutex>
#include "skse/PluginAPI.h"

namespace MerchantRestockFix {
   typedef UInt32 FormID;
   //
   enum { kSaveVersion = 1 };
   static constexpr UInt32 ce_recordSignature = 'Mrch';
   bool Save(SKSESerializationInterface* intfc);
   bool Load(SKSESerializationInterface* intfc, UInt32 version);
};