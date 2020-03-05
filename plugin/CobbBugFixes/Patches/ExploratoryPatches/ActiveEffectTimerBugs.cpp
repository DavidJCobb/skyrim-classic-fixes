#pragma 
#include "ReverseEngineered/Forms/Actor.h"
#include "ReverseEngineered/Objects/ActiveEffect.h"
#include "skse/SafeWrite.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace Exploratory {
         namespace ActiveEffectTimerBugs {
            namespace ConditionUpdate {
               constexpr bool ce_filterStackToSubroutines = true;
               //
               void _dumpStack(RE::ActiveEffect* effect, UInt32* stack) {
                  _MESSAGE("[AE:%08X] Dumping stack:", effect); // list the effect pointer on every entry in case multi-threading interleaves multiple entries
                  for (size_t i = 0; i < 60; i++) {
                     auto content = stack[i];
                     if (ce_filterStackToSubroutines) {
                        if (content & 0xFF000000)
                           continue;
                        if (content < 0x00200000)
                           continue;
                     }
                     _MESSAGE("[AE:%08X] 0x%08X | 0x%08X", effect, &stack[i], content);
                  }
                  _MESSAGE("-------------------");
               }
               void _stdcall Inner(RE::ActiveEffect* effect, float timeDelta, UInt32* stack) {
                  static bool s_didNPCDump = false;
                  //
                  auto target = effect->actorTarget;
                  if (!target)
                     return;
                  auto actor = (RE::Actor*) target->GetTargetActor();
                  if (!actor)
                     return;
                  if (actor == *(RE::Actor**)g_thePlayer || actor == *(RE::Actor**)0x01310588) {
                     if (timeDelta == 0.0F)
                        return;
                     _MESSAGE("[AE:%08X] Checking magic condition update interval for: the player. Time delta is %f.", effect, timeDelta);
                     if (timeDelta > 1.0F)
                        _dumpStack(effect, stack);
                     return;
                  }
                  auto name = CALL_MEMBER_FN(actor, GetReferenceName)();
                  if (!name || !name[0])
                     name = "<unnamed>";
                  _MESSAGE("[AE:%08X] Checking magic condition update interval for: %s. Time delta is %f.", effect, name, timeDelta);
                  if (stack && !s_didNPCDump) {
                     s_didNPCDump = true;
                     _dumpStack(effect, stack);
                  } else if (timeDelta > 1.0F)
                     _dumpStack(effect, stack);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     mov  eax, dword ptr [esp+0x1C];
                     pushad;
                     push esp;
                     push eax;
                     push esi;
                     call Inner; // stdcall
                     popad;
                     cmp byte ptr [esp+0x20], 0; // reproduce patched-over instructions
                     mov  eax, 0x00655C2D;
                     jmp  eax;
                  }
               }
               void Apply() {
                  WriteRelJump(0x00655C28, (UInt32)&Outer);
               }
            }
            namespace AdvanceTime {
            }
            //
            void Apply() {
               ConditionUpdate::Apply();
            }
         }
      }
   }
};