#include "ArmorAddonMO5SFix.h"
#include "skse/SafeWrite.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace ArmorAddonMO5SFix {
         //
         // In the vanilla game, it is impossible to use TextureSets on the 
         // first-person geometry that an ArmorAddon uses for women. This 
         // is because the ARMA subrecord for these texture overrides is 
         // MO5S, but the game checks for MODS due to a typo.
         //
         const uint32_t* casePtr = (uint32_t*)0x004554CE;
         //
         void Apply() {
            if (*casePtr == _byteswap_ulong('MODS'))
               SafeWrite32((UInt32)casePtr, _byteswap_ulong('MO5S'));
         }
      }
   }
}