#include "CrashFixes.h"
#include "skse/SafeWrite.h"
#include "../Services/INI.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace CrashFixes {
         namespace TESIdleFormDestructor {
            //
            // TESIdleForm::~TESIdleForm loops over the contents of the array TESIdleForm::unk20. 
            // For each entry, it sets field unk24 to zero and calls Dispose(true). A user has 
            // sent me a crash report indicating cases where when quitting from the main menu, 
            // the game can crash when entries in this array are nullptr.
            //
            // It is currently not known what the array items are, or what the significance is 
            // of nullptr entries appearing when Bethesda didn't bother checking for them.
            //
            __declspec(naked) void Outer() {
               _asm {
                  test eax, eax;
                  jz   lContinue;
                  mov  dword ptr [eax+0x24], 0; // reproduce patched-over instruction
                  mov  ecx, 0x0055E066; // TESIdleForm::~TESIdleForm+0xE6
                  jmp  ecx;
               lContinue:
                  mov  ecx, 0x0055E070; // TESIdleForm::~TESIdleForm+0xF0
                  jmp  ecx;
               }
            }
            void Apply() {
               if (INI::CrashFixes::TESIdleFormDestructor.bCurrent == false)
                  return;
               WriteRelJump(0x0055E05F, (UInt32)&Outer); // TESIdleForm::~TESIdleForm+0xDF
            }
         }
         //
         void Apply() {
            TESIdleFormDestructor::Apply();
         }
      }
   }
}