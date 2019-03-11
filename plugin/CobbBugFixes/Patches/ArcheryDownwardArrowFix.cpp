#include "ArcheryDownwardArrowFix.h"
#include "ReverseEngineered/Forms/Actor.h"
#include "ReverseEngineered/Forms/Projectile.h"
#include "skse/SafeWrite.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace ArcheryDownwardArrowFix {
         //
         // If you're crouched on a ledge, in third-person, and you try to fire 
         // an arrow downward off the ledge, then the arrow is very likely to hit 
         // the ground at your feet -- as if it spawned at your position instead 
         // of at the tip of your bow. Why is this?
         //
         // Well, when you fire an arrow, the arrow is spawned at a specific node 
         // in your skeleton -- the WEAPON node -- and should then move forward. 
         // However, if you aim at a wall or a floor, that skeleton node could be 
         // past the wall or floor, so Bethesda had to put in some extra code to 
         // guard against that and prevent you from shooting through walls. When 
         // the arrow is mid-flight and your character is performing an action 
         // (i.e. the game checks if you have an ExtraAction), the game will ray-
         // cast from your position to the arrow's position. If the raycast hits 
         // anything, the arrow will be moved to the hit position and will then 
         // collide.
         //
         // So let's draw a picture of what that looks like.
         //                    
         //
         //                           XX
         //                         XXXX
         //                       XXXXXXX
         //                         XXXXXX
         //                       XXXXX
         //                     XXXX  X
         // CCCCCCCCCCCCCCCCCCCCC     XXX
         // CCCCCCCCCCCCCCCCCCCCCCCCCCCCXX
         // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC       AAAA
         // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC          AAAA
         // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC            AAAA
         // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
         // CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
         //
         // The "X"s are your character and the "A"s are the arrow. The "C"s are 
         // the cliff you're standing on. The arrow's position is at its tip, while 
         // your position is centered on the ground beneath you, between your feet. 
         // It's... difficult... to illustrate here and even more difficult to 
         // describe, but the way this works out is that the raycast from your feet 
         // to the arrow hits the ridge you're standing on.
         //
         // In fact, the bug isn't limited to sneaking. If you use TCL to clip half-
         // way into the ground and fire from a standing position, you run into the 
         // same problem.
         //
         // The solution? Move the raycast origin up. We raycast from roughly your 
         // center of mass instead of from your feet, and that ensures that the 
         // raycast more accurately reflects what you should and shouldn't be able 
         // to shoot at.
         //
         void _stdcall Inner(NiPoint3& out, RE::Actor* shooter, RE::Projectile* projectile) {
            out = shooter->pos;
            float height = CALL_MEMBER_FN(shooter, GetComputedHeight)();
            if (height > 0.0F) {
               height *= 0.6; // move from roughly the top of your head to about where you'd have a bow
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
            //
            // You can get to the function we're patching by digging down from ArrowProjectile::Unk_AB. Look 
            // for code that calls a TESObjectREFR method that returns true if the actor has an ExtraAction. 
            // Our patch intercepts arguments for a call to Projectile::MoveToHitPositionIfAny.
            //
            WriteRelJump(0x0079B16F, (UInt32)&Outer);
         }
      }
   }
}