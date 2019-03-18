#include "VampireFeedSoftlock.h"
#include "ReverseEngineered/Forms/TESPackage.h"
#include "ReverseEngineered/Player/PlayerCharacter.h"
#include "skse/SafeWrite.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace VampireFeedSoftlock {
         //
         // Sometimes, the player can softlock while feeding as a vampire. This 
         // happens because of an unknown flaw in the game engine that can cause 
         // the premature deletion of the player's AI package.
         //
         // I should back up a bit. When the player tries to feed as a vampire, 
         // the game handles this by flagging the player as "AI-driven," and 
         // then forcing them to execute an AI package (created at run-time) that 
         // handles feeding. This AI package listens for an animation event from 
         // elsewhere in the executable (specifically, PickNewIdle, which sets 
         // AI process manager flags that the package checks regularly); when the 
         // event is received, the package returns the player to normal and then 
         // removes itself.
         //
         // For some reason, however, it's possible for the AI package to be 
         // removed early by an outside force. This leaves the player stuck: they 
         // are never un-flagged as AI-driven, so they are unable to move or turn. 
         // Essentially, the softlock is the result of the player being turned 
         // into an NPC and never being turned back, because there is no clean-up 
         // mechanism for AI procedures in the engine.
         //
         // TESPackage instances are refcounted, since they can be created at run-
         // time. When a created package is dereferenced, it gets deleted. We hook 
         // the code that destroys the TESPackage; when a vampire-feed package is 
         // destroyed, we check whether the player is AI-driven and whether the 
         // package belongs to them; if so, we remove the player from the AI-driven 
         // state.
         //
         // We have two hooks. We should use ONLY ONE OF THEM at any given moment.
         //
         // EXACT
         //    This patch hooks Struct006F0580::Subroutine006F04F0. It targets a 
         //    specific piece of code that can destroy a TESPackage, and hooks at 
         //    an offset that allows us to check whether that package belonged to 
         //    the player.
         //
         //    In tests, this hook was perfectly sufficient and prevented the 
         //    softlock. However, on the off chance that some other engine bug 
         //    might destroy a vampire-feed package early via a different means, 
         //    I have also kept around an "inexact" hook, which we can switch to 
         //    should the need arise.
         //
         // INEXACT
         //    This patch hooks TESPackage::~TESPackage. It will catch every time 
         //    a TESPackage is destroyed, but it will not have any context as to 
         //    why the package was destroyed or what actor was using it. This 
         //    would potentially lead to conflicts if an NPC vampire feeds while 
         //    the player is intentionally in an AI-driven state (i.e. due to a 
         //    mod that has put them there).
         //
         namespace Exact {
            //
            // This hook patches the specific piece of code that ends up causing  
            // the destruction of a TESPackage. This particular hook site allows 
            // us to check whether the package-to-be-deleted belongs to the player, 
            // because the hook site precedes the code that removes the package 
            // from whatever actor is using it.
            //
            // With this patch, my test save was unable to cause a softlock. (The 
            // test save consisted of me standing over Deekus as he slept in his 
            // bedroll. The save had a high, but not 100%, rate of softlocking when 
            // feeding on him in this position.)
            //
            bool UsesPackage(RE::Actor* actor, RE::TESPackage* package) {
               auto pm = actor->processManager;
               if (pm) {
                  if (pm->unk0C.unk00 == package)
                     return true;
                  auto middle = pm->middleProcess;
                  if (middle && middle->unk30.unk00 == package)
                     return true;
               }
               return false;
            }
            void _stdcall Inner(RE::TESPackage* package) {
               //
               // We patch before the TESPackage destructor is actually called, so 
               // it is safe to access all of the package's fields.
               //
               if (package->type == package->kPackageType_VampireFeed) {
                  //_MESSAGE("Detected the destruction of vampire-feed TESPackage %08X (PACK:%08X).", package, package->formID);
                  //
                  auto player   = *RE::g_thePlayer;
                  if (!player)
                     return;
                  bool isDriven = player->unk726 & 8;
                  if (isDriven && UsesPackage(player, package)) {
                     //_MESSAGE(" - Player is AI-driven and this package belongs to them. Cleaning up.");
                     CALL_MEMBER_FN(player, SetPlayerAIDriven)(false);
                     //_MESSAGE("    - Done.");
                  }
               }
            }
            __declspec(naked) void Outer() {
               _asm {
                  mov  eax,  0x00401710; // reproduce patched-over call to SimpleLock::Lock(const char*)
                  call eax;              //
                  //
                  // Immediately after the patched-over call, we run (TESPackage* edi = this->unk00). 
                  // As it happens, we need that package pointer now, so we'll just do it here too.
                  //
                  mov  edi, dword ptr [esi];
                  push edi;
                  call Inner; // stdcall
                  mov  ecx, 0x006F0555;
                  jmp  ecx;
               }
            }
            void Apply() {
               WriteRelJump(0x006F0550, (UInt32)&Outer); // Struct006F0580::Subroutine006F04F0 + 0x60
            }
         }
         namespace Inexact {
            //
            // The inexact patch hooks TESPackage::~TESPackage, and checks whether 
            // the player is AI-driven. However, it has no way to know whether the 
            // package being deleted actually belongs to the player.
            //
            // There are a few mods for Skyrim that intentionally put the player in 
            // an AI-driven state -- offhand I can think of a mod that automates 
            // the act of walking Skyrim's roads, returning control to the player 
            // if they end up in combat; however, I can't recall the mod's name. 
            // There are also mods that allow NPC vampires to feed on others, and 
            // those vampires will use the same process as the player (minus the 
            // AI-driven flag; they create a package, run it, and then it gets 
            // deleted). This hook would create a conflict between those two mods.
            //
            void _stdcall Inner(RE::TESPackage* package) {
               //
               // We patch halfway into the destructor, before most fields are cleared. 
               // It is safe to access most, maybe all, of TESPackage's fields.
               //
               if (package->type == package->kPackageType_VampireFeed) {
                  auto player   = *RE::g_thePlayer;
                  bool isDriven = player->unk726 & 8; // There's no getter we can call, to my knowledge, but this is the flag the game uses.
                  if (isDriven) {
                     CALL_MEMBER_FN(player, SetPlayerAIDriven)(false); // The game does other stuff besides just setting the flag, so use the setter.
                  }
               }
            }
            __declspec(naked) void Outer() {
               _asm {
                  mov  eax, 0x0043B790; // reproduce patched-over call to DataHandler::IsFormIDNotTemporary
                  call eax;             // 
                  push eax; // protect
                  push esi;
                  call Inner; // stdcall
                  pop  eax; // restore
                  mov  ecx, 0x005E228A;
                  jmp  ecx;
               }
            }
            void Apply() {
               WriteRelJump(0x005E2285, (UInt32)&Outer);
            }
         }
         //
         void Apply() {
            Exact::Apply();
         }
      }
   }
}