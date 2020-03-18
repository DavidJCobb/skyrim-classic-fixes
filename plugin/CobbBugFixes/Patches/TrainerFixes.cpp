#include "TrainerFixes.h"
#include "skse/SafeWrite.h"

#include "Services/INI.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace TrainerFixes {
         namespace IncorrectCountDisplayed {
            //
            // Subroutine {GetTrainingCost} takes a player skill level and returns the cost to train 
            // up from that skill level. It is called by two member functions on TrainingMenu: one 
            // which displays the cost in the UI, and another which actually carries out the trans-
            // action. The latter usese the player's base AV, but the former uses the maximum AV; 
            // this means that if you have a Fortify effect for the relevant skill, the cost that 
            // gets displayed will take that fortification into account and be higher than what you
            // actually need to pay.
            //
            // To fix this, we can just change a call to virtual ActorValueOwner::GetMaximum into a 
            // call to virtual ActorValueOwner::GetBase.
            //
            void Apply() {
               if (CobbBugFixes::INI::TrainerFixes::FixCostUI.bCurrent == false)
                  return;
               SafeWrite8(0x00893780 + 2, 0x0C); // TrainingMenu::Subroutine00893540+0x240
            }
         }
         //
         void Apply() {
            IncorrectCountDisplayed::Apply();
         }
      }
   }
}