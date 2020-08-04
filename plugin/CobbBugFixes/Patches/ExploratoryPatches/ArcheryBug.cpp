#pragma 
#include "ReverseEngineered/Forms/Actor.h"
#include "ReverseEngineered/Forms/Projectile.h"
#include "ReverseEngineered/NetImmerse/nodes.h"
#include "ReverseEngineered/NetImmerse/types.h"
#include "skse/SafeWrite.h"

//
// Attempts to investigate a bug that can cause arrows fired by the 
// player to spawn at their feet, when shooting in third-person while 
// perched on a downward slope and aiming at a downward angle.
//

namespace CobbBugFixes {
   namespace Patches {
      namespace Exploratory {
         namespace ArcheryBug {
            namespace LogActorShotNode {
               void _stdcall Inner(NiNode* node, RE::Actor* actor) {
                  _MESSAGE("Actor [ACHR:%08X] (%08X) is firing a projectile from node %s (%08X).", actor->formID, actor, node->m_name, node);
                  auto& p = actor->pos;
                  _MESSAGE(" - Actor position: (%f, %f, %f)", p.x, p.y, p.z);
                  p = node->m_worldTransform.pos;
                  _MESSAGE(" - Node  position: (%f, %f, %f)", p.x, p.y, p.z);
                  //
                  auto actorNode = actor->GetNiNode();
                  if (actorNode) {
                     _MESSAGE(" - Actor node-70: %s", actorNode->m_name);
                     p = actorNode->worldTransform.pos;
                     _MESSAGE(" - Actor node-70 position: (%f, %f, %f)", p.x, p.y, p.z);
                  }
                  //
                  actorNode = actor->Unk_8D();
                  if (actorNode) {
                     _MESSAGE(" - Actor node-8D: %s", actorNode->m_name);
                     p = actorNode->worldTransform.pos;
                     _MESSAGE(" - Actor node-8D position: (%f, %f, %f)", p.x, p.y, p.z);
                  }
               }
               __declspec(naked) void Outer() {
                  _asm {
                     pushad;
                     push edi;
                     push esi;
                     call Inner; // stdcall
                     popad;
                     lea  eax, [esp + 0x78]; // reproduce patched-over instruction
                     push eax; // reproduce patched-over instruction
                     mov  ecx, 0x004AA89E;
                     jmp  ecx;
                  }
               }
               void Apply() {
                  WriteRelJump(0x004AA899, (UInt32)&Outer);
               }
            }
            namespace LogActorShotProjectile {
               void _stdcall Inner(RE::TESObjectREFR* projectile) {
                  _MESSAGE("Fired projectile is [REFR:%08X] (%08X).", projectile->formID, projectile);
                  if (RE::TESForm* base = projectile->baseForm)
                     _MESSAGE(" - Base form: [FORM:%08X] (%08X)", base->formID, base);
                  auto& p = projectile->pos;
                  _MESSAGE(" - Position: (%f, %f, %f)", p.x, p.y, p.z);
                  //p = projectile->rot;
                  //_MESSAGE(" - Rotation: (%f, %f, %f)", p.x, p.y, p.z);
               }
               __declspec(naked) void Outer() {
                  _asm {
                     push esi;
                     call Inner; // stdcall
                     cmp  dword ptr [esp + 0x54], 0; // reproduce patched-over instruction
                     mov  eax, 0x004AB362;
                     jmp  eax;
                  }
               }
               void Apply() {
                  WriteRelJump(0x004AB35D, (UInt32)&Outer);
               }
            }
            namespace MoveArrowInitialPositionForward {
               //
               // Hypothesis: arrows directly in front of your bow, but are positioned by their 
               // tips, so it's possible for the back of the arrow to impact the ground beneath 
               // you. Can we fix the bug by making them spawn further ahead?
               //
               constexpr float ce_forwardDistance = 58.0F; // arrows are typically 59 or 60 units long

               void _stdcall Inner(NiNode* node, float* pitch, float* yaw, NiPoint3* position) {
                  {  // Reproduce vanilla code
                     //
                     // The vanilla code writes overtop the arguments after pulling them into 
                     // registers and passing them to another call. This prevents our hook from 
                     // being able to catch the arguments. We need to replace the whole function.
                     //
                     struct Shim {
                        DEFINE_MEMBER_FN_LONG(Shim, Subroutine00AAD320, void, 0x00AAD320, float*, float*, float*);
                     };
                     auto  p = (Shim*)&node->m_worldTransform;
                     float f;
                     CALL_MEMBER_FN(p, Subroutine00AAD320)(yaw, pitch, &f);
                  }
                  //
                  // Pitch and yaw are in radians.
                  //
                  auto     matrix = RE::NiMatrix33::ConstructFromEuler(*pitch, 0, *yaw);
                  NiPoint3 dir    = matrix.Forward();
                  dir *= ce_forwardDistance;
                  *position += dir;
                  //
                  // This has the desired effect -- making the arrow start 58 world units ahead 
                  // of where it normally would -- but that doesn't prevent the arrow from colliding 
                  // at the player's feet. Strange. I tried it with double the needed distance, even, 
                  // and that still didn't prevent the collision.
                  //
                  // Well, being able to tamper with arrow trajectories is pretty useful. If we 
                  // ever get raycasts working, we could even add something like aim assist, I 
                  // suppose. For now, though, we need to search elsewhere to fix this bug.
                  //
                  // Debug logging indicates that the arrow DOES spawn at the place we're telling it 
                  // to -- well ahead of the cliff we're standing on during the test. Something must 
                  // either be moving it back under the player, or simulating a trajectory from the 
                  // player to the initial position and "correcting" the arrow upon discovering that 
                  // it "would have" hit -- and it must only be doing that for downward-aimed arrows.
                  //
                  _MESSAGE("Final arrow trajectory:");
                  _MESSAGE(" - Fire from: (%f, %f, %f)", position->x, position->y, position->z);
                  _MESSAGE(" - Pitch: %f; Yaw: %f.", *pitch, *yaw);
               }
               void Apply() {
                  WriteRelJump(0x004D6750, (UInt32)&Inner); // TESObjectREFR::AdjustProjectileFireTrajectory + 0x00
               }
            }
            //
            // Bethesda spawns the arrow at a point just in front of your bow. However, if you're 
            // aiming downward at the ground, this point will be below the ground. In order to 
            // prevent this from allowing you to shoot through the floor, Bethesda checks if the 
            // actor who fired the arrow is performing an action (they don't care what action it 
            // is; they just check for any ExtraAction), and if so, they do a raycast from the 
            // actor's position to the arrow's position. The arrow is moved to the hit position.
            //
            namespace PreventWeirdHavokCall {
               void Apply() {
                  //
                  // Overwrite
                  //
                  //    CALL Projectile::Subroutine007A0090
                  //
                  // with
                  //
                  //    ADD ESP, 8
                  //    NOP
                  //
                  SafeWrite32(0x0079B1A6, 0x9008C483);
                  SafeWrite8 (0x0079B1AA, 0x90);
               }
            }
            namespace AdjustWeirdHavokCall {
               void _stdcall Inner(NiPoint3& out, RE::Actor* shooter, RE::Projectile* projectile) {
                  out = shooter->pos;
                  float height = CALL_MEMBER_FN(shooter, GetComputedHeight)();
                  if (height > 0.0F) {
                     if (CALL_MEMBER_FN(shooter, IsSneaking)())
                        height *= 0.57F; // the executable uses this constant in various places
                  } else {
                     height = 96.0F; // a "default" height used by TESObjectWEAP::Fire
                  }
                  out.z += height;
               }
               __declspec(naked) void Outer() {
                  _asm {
                     lea  eax, [esp + 0x18];
                     push ebp;
                     push edi;
                     push eax;
                     call Inner; // stdcall
                     mov  eax, 0x0079B19B;
                     jmp  eax;
                  }
               }
               void Apply() {
                  WriteRelJump(0x0079B16F, (UInt32)&Outer);
               }
            }
            //
            void Apply() {
               LogActorShotNode::Apply();
               LogActorShotProjectile::Apply();
               //
               //MoveArrowInitialPositionForward::Apply();
               //PreventWeirdHavokCall::Apply();
               AdjustWeirdHavokCall::Apply();
            }
         }
      }
   }
};