#include "ModArmorWeightPerk.h"
#include "skse/SafeWrite.h"
#include "skse/GameData.h" // CalculatePerkData
#include "ReverseEngineered/Forms/Actor.h"
#include "ReverseEngineered/Forms/TESForm.h"
#include "ReverseEngineered/ExtraData.h"

#include "Services/INI.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace ModArmorWeightPerk {
         namespace InitialItemsUnaffected {
            //
            // Subroutine {float ExtraContainerChanges::Data::GetTotalWeight()} is used to compute the total 
            // carry weight of all of an actor's items. However, it has a flaw. It first loops over all of 
            // the items that the actor originally spawns with (not including outfit items) by way of the 
            // ActorBase's TESContainer; then, it loops over ExtraContainerChanges. It only applies the 
            // "Mod Armor Weight" perk, used to power the Steed Stone bonus, in the latter loop, so any item 
            // forms that initially appear in the actor's inventory won't be affected by the perk.
            //
            // In practice, in a vanilla game, this prevents the Steed Stone from making an equipped Iron 
            // Shield weightless.
            //
            // Our solution is to patch the former loop to use similar logic to the latter loop.
            //
            void _stdcall Inner(RE::Actor* subject, RE::InventoryEntryData* entry, float weight, float& total, UInt32& count) {
               auto form = entry->type;
               if (!form || form->formType != kFormType_Armor)
                  return;
               if (count <= 0)
                  return;
               if (CALL_MEMBER_FN(entry, IsWorn)()) {
                  CalculatePerkData(PerkEntryPoints::kEntryPoint_Mod_Armor_Weight, (::TESObjectREFR*)subject, form, &weight);
                  total += weight;
                  //
                  // The player is indeed wearing one of these. We've already added the perk-altered weight 
                  // for the worn item to the total, so subtract the count so that the unaltered weight is 
                  // only used for the rest of the stack.
                  //
                  --count;
               }
            }
            __declspec(naked) void Outer() {
               //
               // The weight of the current item form is on the FPU stack. Register 
               // esi holods an InventoryEntryData pointer.
               //
               _asm {
                  lea  edx, dword ptr [esp + 0x28]; // total count
                  lea  eax, dword ptr [esp + 0x10]; // total weight
                  mov  ecx, dword ptr [esp + 0x24]; // Actor*
                  push edx;
                  push eax;
                  mov  eax, dword ptr [esp + 0x1C]; // current item form's weight: &esp14 + 8
                  push eax;
                  push esi; // InventoryEntryData*
                  push ecx;
                  call Inner; // stdcall
                  fimul dword ptr [esp + 0x28]; // reproduce patched-over instruction
                  fadd  dword ptr [esp + 0x10]; // reproduce patched-over instruction
                  mov   eax, 0x0047B6E1;
                  jmp   eax;
               }
            }
            void Apply() {
               if (CobbBugFixes::INI::ModArmorWeightPerk::FixInitial.bCurrent == false)
                  return;
               WriteRelJump(0x0047B6D9, (UInt32)&Outer); // ExtraContainerChanges::Data::GetTotalWeight+0xE9
               SafeWrite16 (0x0047B6D9 + 5, 0x9090); // NOP
               SafeWrite8  (0x0047B6D9 + 7, 0x90);   // NOP
            }
         }
         namespace EntireStacksWronglyAffected {
            //
            // Subroutine {float ExtraContainerChanges::Data::GetTotalWeight()} does this when 
            // looping over any items that weren't present in the container initially:
            //
            //    float item_weight = GetFormWeight(item);
            //    if (item->formType == kFormType_Armor) {
            //       if (entry->IsWorn()) {
            //          CalculatePerkData(kEntryPoint_Mod_Armor_Weight, me, item, &item_weight); // at 0x0047B7D9
            //          --count;
            //          this->totalWeight += item_weight;
            //       }
            //    }
            //    this->totalWeight += item_weight * count;
            //
            // See the problem? Bethesda wanted Mod Armor Weight to only apply to the one of the 
            // item that you're actually wearing, but since they modified the item_weight float 
            // that gets used in both branches, it applies to the whole stack. This is the cause 
            // of the Steed Stone making an entire stack of an item weightless. The solution is 
            // to use a different float that is initialized to be a copy of item_weight.
            //
            static float f_weight;
            __declspec(naked) void OuterArg() {
               _asm {
                  mov  eax, dword ptr [esp + 0x14];
                  mov  [f_weight], eax;
                  lea  eax, f_weight; // replace instruction
                  push eax; // reproduce patched-over instruction
                  mov  eax, 0x0047B7D5;
                  jmp  eax;
               }
            }
            __declspec(naked) void OuterAfter() {
               _asm {
                  fld  dword ptr [esp + 0x30]; // reproduce patched-over instruction
                  fadd dword ptr [f_weight];
                  mov  eax, 0x0047B7E6;
                  jmp  eax;
               }
            }
            void Apply() {
               if (CobbBugFixes::INI::ModArmorWeightPerk::FixStacks.bCurrent == false)
                  return;
               WriteRelJump(0x0047B7D0, (UInt32)&OuterArg);
               WriteRelJump(0x0047B7DE, (UInt32)&OuterAfter);
               SafeWrite16 (0x0047B7DE + 5, 0x9090); // NOP
               SafeWrite8  (0x0047B7DE + 7, 0x90);   // NOP
            }
         }
         //
         void Apply() {
            InitialItemsUnaffected::Apply();
            EntireStacksWronglyAffected::Apply();
         }
      }
   }
}