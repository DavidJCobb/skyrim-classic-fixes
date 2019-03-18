#include "UnderwaterAmbienceCellBoundaryFix.h"
#include "skse/SafeWrite.h"

#include "Services/INI.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace UnderwaterAmbienceCellBoundaryFix {
         //
         // When the camera is underwater, the game uses an "underwater" imagespace 
         // (supplied by the water type) and an "underwater" ambience (supplied by 
         // a DOBJ). Every time the camera crosses a cell boundary, the underwater 
         // ambient sound loop will stop and restart (i.e. you will hear the sounds 
         // that play when the camera exits and enters water, despite it not having 
         // done so).
         //
         void Apply() {
            if (CobbBugFixes::INI::UnderwaterAmbienceCellBoundaryFix::Enabled.bCurrent == false) {
               return;
            }
            //
            // We are patching the middle of BGSWaterCollisionManager::bshkAutoWater::Unk_0A, 
            // to no-op a call to {this->unkE0.TESV_00632DF0(Arg1)}. The purpose of the call 
            // isn't clear, but it is one of four calls (all from Havok-based water physics 
            // systems) that is ultimately responsible for making the game consider the 
            // camera to be underwater or not underwater. This particular check appears to 
            // cause false-negatives at cell boundaries for some reason.
            //
            // No-oping this check has no readily observable consequences beyond fixing the 
            // bug, but I must assume that the check is made for a reason -- and whatever 
            // reason that is will now be unaccounted for. This is the main reason why this 
            // patch is gated on an INI setting: if there are unexpected and severe issues 
            // that might stem from this, I'd like to be able to ship a "fix" as soon as 
            // possible in the form of just replacing an INI file rather than rebuilding 
            // the whole mod.
            //
            SafeWrite8(0x00633265 + 0, 0x83); // ADD ESP, 4
            SafeWrite8(0x00633265 + 1, 0xC4);
            SafeWrite8(0x00633265 + 2, 0x04);
            SafeWrite8(0x00633265 + 3, 0x90); // NOP
            SafeWrite8(0x00633265 + 4, 0x90); // NOP
         };
      }
   }
}