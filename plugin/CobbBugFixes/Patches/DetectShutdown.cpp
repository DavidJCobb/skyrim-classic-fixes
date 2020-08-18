#include "DetectShutdown.h"
#include "skse/SafeWrite.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace DetectShutdown {
         extern bool is_shutting_down = false;
         //
         // This is used by the crash logger, so that it can tell that a crash has occurred 
         // after the game has mostly shut down.
         //
         using handler_t = void(*)();
         static handler_t prior = nullptr;

         void _stdcall Outer() {
            is_shutting_down = true;
            _MESSAGE("Detected that the game is shutting down...");
            if (prior)
               (prior)();
         }
         void Apply() {
            constexpr uint32_t address = 0x0069E864; // address of a CALL to a no-op function, near the end of the game's main()
            prior = (handler_t) (*(uint32_t*)(address + 1) + address + 5);
            WriteRelCall(address, (UInt32)&Outer);
         }
      }
   }
}