#include "NPCTorchLandscapeFix.h"
#include "skse/SafeWrite.h"
#include "ReverseEngineered/Forms/TESForm.h"

#include "Services/INI.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace NPCTorchLandscapeFix {
         //
         // Torches held by NPCs don't cast light on the landscape, or at least don't do it 
         // consistently. Torches held by the player seem to always light the landscape. Why is 
         // this?
         //
         // There's a function which sets up lighting parameters based on an input TESObjectREFR, 
         // which can be either a light or an actor holding a torch. NPCs that are holding torches 
         // happen to have form flag 0x00020000, which on light references would be the "doesn't 
         // light landscape" flag. I don't know if actors are actually intended to have lighting 
         // flags, or if that flag has some other meaning on actors and Bethesda didn't check the 
         // form type along with the flag. In the former case, it would mean that actors can be 
         // hardcoded not to light the landscape (why??); in the latter case, Bethesda made a 
         // mistake.
         // 
         // Safest fix is to ignore the flag if the form is an actor, though this means that if 
         // someone makes a torch that is *actually* set to not light the landscape, we're gonna 
         // make it light the landscape anyway.
         //
         UInt32 _stdcall Inner(RE::TESForm* lightSource) {
            //
            // We patch halfway into the destructor, before most fields are cleared. 
            // It is safe to access most, maybe all, of TESPackage's fields.
            //
            if (lightSource->formType == 0x3E)
               return 0;
            return (lightSource->flags >> 0x11); // reproduce patched-over opcode (check for "doesn't light landscape" flag) but, only conditionally
         }
         __declspec(naked) void Outer() {
            _asm {
               push eax; // protect
               push edx; // protect
               push ebp;
               call Inner; // stdcall
               mov  ecx, eax;
               pop  edx; // restore
               pop  eax; // restore
               shr  eax, 8; // reproduce patched-over opcode
               retn;
            }
         }
         void Apply() {
            if (CobbBugFixes::INI::NPCTorchLandscapeFix::Enabled.bCurrent == false)
               return;
            WriteRelCall(0x0049E00D, (UInt32)&Outer); // use a call since there are no registers available to jump back with // circa TESObjectLIGH::sub0049DC10+3FF
            SafeWrite8(0x0049E00D + 5, 0x90); // this NOP is REQUIRED since we're using a call
         }
      }
   }
}