#pragma once

namespace CobbBugFixes {
   namespace Patches {
      namespace DetectShutdown {
         extern bool is_shutting_down;
         //
         void Apply();
      }
   }
}