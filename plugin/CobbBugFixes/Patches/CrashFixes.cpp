#include "CrashFixes.h"
#include "skse/SafeWrite.h"
#include "../Services/INI.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace CrashFixes {
         namespace Singleton012E2CF8_Unk68_Subroutine006483F0 {
            //
            // Sometimes, this singleton can be deleted, but the following call still 
            // gets made for... I don't know what reason... during shutdown.
            //
            //    (*(0x012E640C))->unk68.sub006483F0(eax);
            //
            // Per our crash logs, the bad call is coming from ~BSFaceGenAnimationData().
            //
            __declspec(naked) void Outer() {
               _asm {
                  cmp  ebp, 0x00000068; // if the singleton is nullptr, then &singleton->unk68 == offsetof(decltype(singleton), unk68)
                  je   lReturn;
                  cmp  eax, 0x30000001;
                  mov  edx, 0x0064840D;
                  jmp  edx;
               lReturn:
                  mov  edx, 0x0064847B;
                  jmp  edx;
               }
            }
            void Apply() {
               WriteRelJump(0x00648408, (UInt32)&Outer);
            }
         }
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
            Singleton012E2CF8_Unk68_Subroutine006483F0::Apply();
            TESIdleFormDestructor::Apply();
         }
      }
   }
}