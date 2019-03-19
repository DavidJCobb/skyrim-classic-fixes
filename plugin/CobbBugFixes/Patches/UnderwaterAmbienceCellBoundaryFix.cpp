#include "UnderwaterAmbienceCellBoundaryFix.h"
#include "ReverseEngineered/Forms/TESObjectCELL.h"
#include "ReverseEngineered/Forms/TESWorldSpace.h"
#include "ReverseEngineered/Player/PlayerCharacter.h"
#include "ReverseEngineered/Systems/TESCamera.h"
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
         static bool s_isAutoWaterCheck = false;
         //
         __declspec(naked) void bhk_Outer() {
            _asm {
               mov  s_isAutoWaterCheck, 1;
               mov  eax, 0x00632DF0;
               call eax;
               mov  s_isAutoWaterCheck, 0;
               mov  ecx, 0x0063326A;
               jmp  ecx;
            };
         };

         namespace UnderwaterFX {
            bool _stdcall Inner() {
               if (!s_isAutoWaterCheck)
                  return false;
               auto player = *RE::g_thePlayer;
               auto camera = RE::PlayerCamera::GetInstance();
               if (!player || !camera)
                  return false;
               auto cell = player->parentCell;
               if (!cell)
                  return false;
               auto world = cell->parentWorld;
               if (!world)
                  return false;
               cell = CALL_MEMBER_FN(world, GetCellThatContainsPoint)(&camera->unkB4);
               if (!cell)
                  return false;
               float water = CALL_MEMBER_FN(cell, GetWaterLevel)();
               float camZ = camera->unkB4.z;
               return camZ >= water;
            }
            __declspec(naked) void Outer() {
               _asm {
                  push ecx; // protect
                  call Inner;
                  pop  ecx; // restore
                  test al, al;
                  jz   lSkip;
                  mov  eax, 0x00635240;
                  call eax;
                  jmp  lExit;
               lSkip:
                  add  esp, 0x8;
               lExit:
                  mov  eax, 0x00632E5E;
                  jmp  eax;
               }
            }
            void Apply() {
               WriteRelJump(0x00632E59, (UInt32)&Outer);
            }
         };


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
            // This check is also responsible for properly handling cell transitions when a 
            // cell's water height varies. Simply disabling the check will cause you to swim 
            // in midair if you walk from a cell with a high water height to a cell with a 
            // low water height (or rather, you'll swim if you touch the previous cell's 
            // water height). As such, we need to patch it very carefully: we need to prevent 
            // it from disabling underwater FX, while still letting it do everything else. 
            //
            // Our patch covers most cases. There is one known issue: if you swim "out" the 
            // side of the water, the underwater FX will persist while you're on land. That 
            // is, if you do this:
            //
            //                        ____________________
            //
            //
            //                        <------ ( You )
            //
            //
            //    ____________________
            //
            // then you'll end up in the air, but with underwater FX. This shouldn't be 
            // possible in a properly-constructed worldspace; generally, Bethesda places 
            // animated waterfalls that are actually solid, to force you out of the water 
            // before this can occur.
            //
            UnderwaterFX::Apply();
            WriteRelJump(0x00633265, (UInt32)&bhk_Outer);
         };
      }
   }
}