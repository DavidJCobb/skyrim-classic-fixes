#pragma once
#include "ReverseEngineered/Forms/Actor.h"
#include "ReverseEngineered/Forms/TESPackage.h"
#include "ReverseEngineered/Player/PlayerCharacter.h"
#include "ReverseEngineered/Systems/BSTEvent.h"
#include "skse/SafeWrite.h"

struct _PackageListener : RE::BSTEventSink<RE::TESPackageEvent> {
   //
   // To use this, set up an SKSE messaging listener and at some point during load 
   // (I used DataLoaded), do:
   //
   //    auto holder = RE::BSTEventSourceHolder::GetOrCreate();
   //    CALL_MEMBER_FN(&holder->package, AddEventSink)(_PackageListener::GetInstance());
   //
   // That said, the player doesn't fire package events, at least for vampirism.
   //
   virtual EventResult Handle(void* aEv, void* aSource) override {
      auto ev = convertEvent(aEv);
      auto source = convertSource(aSource);
      //
      if (!ev) {
         _MESSAGE("No event!");
         return EventResult::kEvent_Continue;
      }
      if (ev->eventType == ev->kType_PackageStart) {
         return EventResult::kEvent_Continue;
      }
      auto player = ((RE::PlayerCharacter*) *g_thePlayer);
      if ((RE::Actor*) ev->target != (RE::Actor*) player) {
         auto target = (RE::Actor*) ev->target;
         _MESSAGE("Package event on a non-player actor %08X.", target);
         if (target)
            _MESSAGE(" - Actor %08X named %s.", target->formID, target->GetFullName(0));
         return EventResult::kEvent_Continue;
      }
      _MESSAGE("Package end/change event on the player! Event Type: %d; ID: %08X", ev->eventType, ev->packageFormID);
      //
      if (ev->packageFormID) {
         auto pack = (RE::TESPackage*) LookupFormByID(ev->packageFormID);
         if (pack) {
            if (pack->type == pack->kPackageType_VampireFeed) {
               _MESSAGE(" - Package is vampire-feed.");
               bool isDriven = player->unk726 & 8;
               if (isDriven) {
                  _MESSAGE(" - Player is AI-driven. Cleaning up.");
                  CALL_MEMBER_FN(player, SetPlayerAIDriven)(false);
                  _MESSAGE("    - Done.");
               }
            }
         } else {
            _MESSAGE(" - Package irretrievable.");
         }
      }
      return EventResult::kEvent_Continue;
   };
   //
   static _PackageListener* GetInstance() {
      static _PackageListener instance;
      return &instance;
   };
};

namespace CobbBugFixes {
   namespace Patches {
      namespace Exploratory {
         namespace VampireFeedSoftlock {
            namespace BSTEventSink_006D21F0 {
               void _stdcall Inner(RE::Actor* actor, const char** eventName) {
                  if (actor != (RE::Actor*) *g_thePlayer)
                     return;
                  _MESSAGE("Intercepted BSTEventSink<BSAnimationGraphEvent>::Subroutine006D21F0 on the player.");
                  if (eventName && *eventName)
                     _MESSAGE(" - Animation event is: %s", *eventName);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     lea  eax, [esi - 0x1C];
                     push edi; // original const char* arg1
                     push eax;
                     call Inner; // stdcall
                     lea  eax, [esp + 0x24]; // reproduce patched-over instruction
                     push eax;               // reproduce patched-over instruction
                     mov  ecx, 0x006D2232;
                     jmp  ecx;
                  }
               }
               void Apply() {
                  WriteRelJump(0x006D222D, (UInt32)&Outer);
               }
            }
            namespace BSResponse_006CE350 {
               void _stdcall Inner(RE::Actor* actor, UInt32* response) {
                  if (!actor || actor != (RE::Actor*) *g_thePlayer)
                     return;
                  _MESSAGE("Intercepted BSResponse::Unk_01 on the player. Response VTBL is %08X.", *response);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     call eax;
                     mov  bl, al;
                     mov  eax, dword ptr [esp + 0x1C];
                     push esi;
                     push eax;
                     call Inner; // stdcall
                     mov  eax, 0x006CE3E5;
                     jmp  eax;
                  }
               }
               void Apply() {
                  WriteRelJump(0x006CE3AE, (UInt32)&Outer);
               }
            }
            namespace PickNewIdleHandler_Unk01 {
               void _stdcall Inner(RE::Actor* actor) {
                  if (!actor || actor != (RE::Actor*) *g_thePlayer)
                     return;
                  _MESSAGE("Intercepted PickNewIdleHandler::Unk_01 on the player. Process manager is %08X.", actor->processManager);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     push eax; // protect
                     push eax;
                     call Inner; // stdcall
                     pop  eax; // restore
                     mov  ecx, dword ptr [eax + 0x88]; // reproduce patched-over instruction
                     mov  edx, 0x00780E5A;
                     jmp  edx;
                  }
               }
               void Apply() {
                  WriteRelJump(0x00780E54, (UInt32)&Outer);
               }
            }
            //
            namespace ActorProcessManager_FailCase01 {
               void _stdcall Inner() {
                  _MESSAGE("[VAMPIRE FEED] Failure case hit: drawing/sheathing weapon.");
               }
               __declspec(naked) void Outer() {
                  _asm {
                     call edx;
                     call Inner; // stdcall
                     pop  edi; // reproduce patched-over instructions...
                     pop  esi;
                     add  esp, 8;
                     retn 4;
                  };
               }
               void Apply() {
                  WriteRelJump(0x0070CDA7, (UInt32)&Outer);
               }
            }
            namespace ActorProcessManager_Check01 {
               void _stdcall Inner(bool flag) {
                  _MESSAGE("[VAMPIRE FEED] Unk9A flag 02: %d", flag);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     mov  eax, 0x006F4670;
                     call eax;
                     push eax; // protect
                     push eax;
                     call Inner; // stdcall
                     pop  eax; // restore
                     mov  ecx, 0x0070CE8E;
                     jmp  ecx;
                  };
               }
               void Apply() {
                  WriteRelJump(0x0070CE89, (UInt32)&Outer);
               }
            }
            namespace ActorProcessManager_Check02 {
               void _stdcall Inner(bool flag) {
                  _MESSAGE("[VAMPIRE FEED] Idle completion status: %d", flag);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     push ecx; // protect
                     mov  eax, 0x006FB550;
                     call eax;
                     push eax; // protect
                     push eax;
                     call Inner; // stdcall
                     pop  eax; // restore
                     test al, al; // test what we need to test
                     pop  ecx; // restore
                     mov  eax, 0x0070CE9B;
                     jmp  eax;
                  };
               }
               void Apply() {
                  WriteRelJump(0x0070CE94, (UInt32)&Outer);
               }
            }
            //
            /*//
            namespace TESPackage_Destructor {
               void _stdcall Inner(RE::TESPackage* package, UInt32* esp) {
                  if (package->type == package->kPackageType_VampireFeed) {
                     _MESSAGE("Detected the destruction of vampire-feed TESPackage %08X (PACK:%08X).", package, package->formID);
                     //
                     auto player   = *RE::g_thePlayer;
                     bool isDriven = player->unk726 & 8;
                     if (isDriven) {
                        _MESSAGE(" - Player is AI-driven. Cleaning up.");
                        CALL_MEMBER_FN(player, SetPlayerAIDriven)(false);
                        _MESSAGE("    - Done.");
                     }
                     //
                     _MESSAGE(" - Logging 100 dwords from the stack...");
                     for (UInt8 i = 0; i < 100; i++) {
                        _MESSAGE("    - [esp + 0x%02X] == %08X", (i * 4), esp[i]);
                     }
                  }
               }
               __declspec(naked) void Outer() {
                  _asm {
                     mov  eax, 0x0043B790; // reproduce patched-over call to DataHandler::IsFormIDNotTemporary
                     call eax;
                     push eax; // protect
                     push esp;
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
            //*/
            namespace PackageFree {
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
                  if (package->type == package->kPackageType_VampireFeed) {
                     _MESSAGE("Detected the destruction of vampire-feed TESPackage %08X (PACK:%08X).", package, package->formID);
                     //
                     auto player   = *RE::g_thePlayer;
                     if (!player)
                        return;
                     bool isDriven = player->unk726 & 8;
                     if (isDriven && UsesPackage(player, package)) {
                        _MESSAGE(" - Player is AI-driven and this package belongs to them. Cleaning up.");
                        CALL_MEMBER_FN(player, SetPlayerAIDriven)(false);
                        _MESSAGE("    - Done.");
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
                  WriteRelJump(0x006F0550, (UInt32)&Outer);
               }
            }
            //
            void Apply() {
               BSTEventSink_006D21F0::Apply();
               BSResponse_006CE350::Apply();
               PickNewIdleHandler_Unk01::Apply();
               //
               ActorProcessManager_FailCase01::Apply();
               ActorProcessManager_Check01::Apply();
               ActorProcessManager_Check02::Apply();
               //
               /*TESPackage_Destructor::Apply();*/
               PackageFree::Apply();
            }
         }
      }
   }
}