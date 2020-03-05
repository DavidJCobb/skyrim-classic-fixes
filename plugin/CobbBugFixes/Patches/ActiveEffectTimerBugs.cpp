#include "ActiveEffectTimerBugs.h"
#include "skse/SafeWrite.h"
#include "ReverseEngineered/Objects/ActiveEffect.h"
#include "ReverseEngineered/Systems/012E32E8.h"
#include "ReverseEngineered/GameSettings.h"
#include "Services/INI.h"

#define COBB_ACTIVE_EFFECT_TIMER_FIX_DEBUG 0

#if COBB_ACTIVE_EFFECT_TIMER_FIX_DEBUG == 1
   #pragma message("WARNING: You have left the Active Effect Timer Bugfix's debugging code on!")
   //
   // The debug flag forces our alternate timer behavior for all effects rather than just the 
   // ones that need it; it also logs all timer rollovers and all condition updates. That will 
   // be very slow and consume a huge amount of hard drive space during a normal play session.
   //
#endif

namespace CobbBugFixes {
   namespace Patches {
      namespace ActiveEffectTimerBugs {
         //
         // There are two major bugs with active effects that concern the effects of floating-point 
         // imprecision on their internal "elapsed time" values. In both cases, the bugs stem from 
         // attempting to add a very small frame duration (e.g. 0.016 for 60FPS) to a very large 
         // elapsed time value. The threshold at which elapsed time cannot be reliably incremented 
         // will vary according to your frame rate, with 120FPS users seeing problems after 131072 
         // elapsed seconds (about a day and a half), 60FPS users seeing problems after 262144 
         // elapsed seconds (about three days), and 30FPS users seeing problems after 524288 elapsed 
         // seconds (about six days).
         //
         // The two problems are:
         //
         //  - Elapsed time itself cannot be reliably incremented, so temporary effects with 
         //    especially long durations will be effectively permanent.
         //
         //  - The condition interval check fails due to floating-point imprecision, so the game 
         //    does not re-check conditions on long-running active effects. This occurs even if the 
         //    effects are supposed to be permanent, e.g. ability spells.
         //
         //    The game re-checks conditions when the following statement is true:
         //
         //       (uint32_t)(elapsed / interval) != (uint32_t)((elapsed + frameTime) / interval)
         //
         //    Presumably the game does it this way to stagger out condition updates, so that the 
         //    game doesn't have to run a huge number of conditions all on the same frame and 
         //    hitch.
         //
         // There is an additional wrinkle as well: the time involved is real-time with simulation. 
         // An example: if you have the default timescale of 20, then 900 real-world seconds will 
         // represent five in-game hours; accordingly, if you skip five in-game hours by waiting, 
         // then the conversion is performed in reverse and all actors (and their effects) skip 
         // ahead by 900 real-world seconds.
         //
         // Our solution is to use a global timer which rolls over to zero shortly after hitting 
         // the condition update interval. We patch the code which increments elapsed time and the 
         // code which checks the condition update interval, and we use our global timer for any 
         // active effects whose elapsed times have hit the unsafe threshold for 120FPS. The 
         // specific behaviors for these effects are:
         //
         //  - For condition checks, we run conditions if the timer exceeds the condition 
         //    interval, e.g. a value of 1.015. This should allow us to properly re-check 
         //    conditions independently of an effect's duration.
         //
         //  - For elapsed time handling, we just increment the value  at a more granular rate. 
         //    The idea is that if floating-point imprecision doesn't allow us to increment the 
         //    elapsed time by 0.016 every 0.016 seconds, then we'll just increment it by 1 
         //    every 1 second. This should allow temporary effect durations to work properly out 
         //    to about 16777216 seconds, or about 194 days of real time and simulated real time.
         //
         // Our global timer is synchronized to g_globalActorTimer, which is used by Actors to 
         // pass real-world time. Actors remember the last time they've checked it, and they use 
         // that to compute a delta which is passed downstream, eventually making its way to 
         // ActiveEffect::AdvanceTime. We have found and hooked the function that updates the 
         // global actor timer, so that our own timer matches it perfectly.
         // 
         // ---------------------------------------------------------------------------------------
         //
         // This patch relies on the following assumptions:
         //
         //  - All actors in high process will check the g_globalActorTimer every frame. This super 
         //    SHOULD happen: one of the AI linear task threads' functions calls a method on the AI 
         //    singleton (Unknown012E32E8::AdvanceHighProcessActorTime) which loops over all actors 
         //    in high process and calls Actor::AdvanceTime(0).
         //
         //  - Actors in low process are irrelevant in the vanilla game, so we don't need a solution 
         //    that works for them. Because we're using a very short looping timer, our approach is 
         //    untenable for actors in low process: if they "miss" a large block of simulated time, 
         //    there's no way to tell them about it when they're loaded later.
         //

         static float s_timer = 0.0;
         static float s_safetyThreshold = 131072.0F; // number of seconds at which we can no longer reliably add 120FPS frame time

         inline float _getInterval() {
            return RE::GMST::fActiveEffectConditionUpdateInterval->data.f32;
         }

         namespace ManageTimer {
            void _stdcall Inner(float set_to) {
               float& existing = *RE::g_globalActorTimer;
               float  delta    = set_to - existing;
               if (delta == 0.0F)
                  return;
               if (delta < 0.0F) {
                  s_timer = 0.0F;
                  return;
               }
               if (s_timer > _getInterval()) {
                  //
                  // We run this check before incrementing the timer; that way, we're pretty much guaranteed to 
                  // increase the timer past the interval for one frame, before resetting it on the next. That's 
                  // what we want; it's what we need in order for the other patches (which read the timer) to be 
                  // aware of when the interval's been reached.
                  //
                  #if COBB_ACTIVE_EFFECT_TIMER_FIX_DEBUG == 1
                     _MESSAGE("[%012d] Timer rollover from %f. Delta to be added is %f.", GetTickCount(), s_timer, delta);
                  #endif
                  s_timer = 0.0F;
               }
               s_timer += delta;
            }
            __declspec(naked) void Outer() {
               _asm {
                  fld  dword ptr [esp + 0x4];      // reproduce patched-over instruction
                  mov  eax, dword ptr [esp + 0x4]; // reproduce patched-over instruction
                  push eax; // protect
                  push eax;
                  call Inner; // stdcall
                  pop  eax; // restore
                  mov  ecx, 0x00753F98;
                  jmp  ecx;
               }
            }
            void Apply() {
               WriteRelJump(0x00753F90, (UInt32)&Outer); // Unknown012E32E8::SetActorGlobalTimer+0x00 i.e. the start of that subroutine
               SafeWrite16 (0x00753F90 + 5, 0x9090); // NOP
               SafeWrite8  (0x00753F90 + 7, 0x90);
            }
         }
         namespace ActiveEffectAdvanceTime {
            void _stdcall Inner(float timeDelta, RE::ActiveEffect* effect, RE::Actor* target) {
               #if COBB_ACTIVE_EFFECT_TIMER_FIX_DEBUG != 1
                  if (effect->elapsed < s_safetyThreshold) {
                     effect->elapsed += timeDelta; // vanilla behavior
                     return;
                  }
               #endif
               //
               // Code below runs if the elapsed time is now too high to track accurately.
               //
               if (s_timer >= _getInterval())
                  effect->elapsed += s_timer;
                  //
                  // NOTE: This will break somewhat if Actor::AdvanceTime ever gets called multiple 
                  // times in a single frame, though from what I've seen that shouldn't happen unless 
                  // it's also skipping time (see above documentation on the simulated passage of 
                  // real time). You could avoid that extremely-hypothetical breakage by instead 
                  // incrementing the elapsed time by the rollover threshold when the timer is at 
                  // zero; however, that will prevent you from properly handling skipped time.
                  //
            }
            __declspec(naked) void Outer() {
               //
               // The time to elapse is loaded onto the FPU stack. Register [esi] holds an 
               // ActiveEffect instance. [esp+0x0C] should hold the target as an Actor.
               //
               _asm {
                  mov  eax, dword ptr [esp+0xC];
                  push ecx; // protect
                  push eax;
                  push esi;
                  push ecx; // make room for float argument
                  fstp [esp];
                  call Inner; // stdcall
                  pop  ecx; // restore
                  mov  eax, 0x0000656F66;
                  jmp  eax;
               }
            }
            void Apply() {
               WriteRelJump(0x00656F60, (UInt32)&Outer); // ActiveEffect::AdvanceTime+0x2B0
               SafeWrite8  (0x00656F60 + 5, 0x90); // NOP
            }
         }
         namespace ActiveEffectConditionInterval {
            //
            // NOTE: ActiveEffect::AdvanceTime calls ActiveEffect::Unk_05 (which eventually calls 
            // ActiveEffect::DoConditionUpdate) before it advances the elapsed time.
            //
            bool _stdcall Inner(RE::ActiveEffect* effect, float delta) { // returns true if we need to re-run conditions
               double elapsed = effect->elapsed;
               double setting = RE::GMST::fActiveEffectConditionUpdateInterval->data.f32;
               #if COBB_ACTIVE_EFFECT_TIMER_FIX_DEBUG != 1
                  if (elapsed < s_safetyThreshold)
                     return (UInt32)(elapsed / setting) != (UInt32)((elapsed + delta) / setting);
               #endif
               //
               // Code below runs if the elapsed time is now too high to track accurately.
               //
               #if COBB_ACTIVE_EFFECT_TIMER_FIX_DEBUG
                  auto actor = effect->actorTarget->GetTargetActor();
                  if (actor && actor == *g_thePlayer) {
                     if (s_timer > setting) {
                        const char* name = "<unknown>";
                        {
                           auto ei = effect->effect;
                           if (ei) {
                              auto mgef = ei->mgef;
                              if (mgef) {
                                 auto n = mgef->fullName.name;
                                 if (n.data)
                                    name = n.data;
                              }
                           }
                        }
                        _MESSAGE("[%012d] [AE:%08X:%s] Delta %f. Player will recheck conditions at timer %f / interval %f.", GetTickCount(), effect, name, delta, s_timer, setting);
                        return true;
                     }
                     return false;
                  }
               #endif
               return (s_timer > setting);
            }
            __declspec(naked) void Outer() {
               //
               // Register [esi] holds an ActiveEffect pointer.
               //
               _asm {
                  mov  eax, dword ptr[esp + 0x1C]; // Arg1 timeDelta
                  push eax;
                  push esi;
                  call Inner; // stdcall. returns true if we need to re-run conditions
                  test al, al;
                  jz   lExit;
                  mov  eax, 0x00655C87;
                  jmp  eax;
               lExit:
                  mov  eax, 0x00655D04;
                  jmp  eax;
               }
            }
            void Apply() {
               WriteRelJump(0x00655C2F, (UInt32)&Outer); // ActiveEffect::DoConditionUpdate+0x8F
               SafeWrite32 (0x00655C2F + 5, 0x90909090); // NOP
            }
         }
         //
         void Apply() {
            if (!INI::ActiveEffectTimerFixes::Enabled.bCurrent)
               return;
            ManageTimer::Apply();
            ActiveEffectAdvanceTime::Apply();
            ActiveEffectConditionInterval::Apply();
         }
      }
   }
}