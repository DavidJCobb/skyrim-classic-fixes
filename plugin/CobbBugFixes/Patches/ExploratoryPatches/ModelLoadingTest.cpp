#include "ModelLoadingTest.h"
#include "ReverseEngineered\Shared.h"
#include "skse/NiNodes.h"
#include "skse/NiTypes.h"

namespace CobbBugFixes {
   namespace Patches {
      namespace Exploratory {
         namespace ModelLoadingTest {
            struct LoadModelOptions {
               UInt32 unk00 = 3;
               UInt32 unk04 = 3;
               bool   unk08 = false;
               bool   unk09 = false; // "is facegen head?" "is helmet?"
               bool   unk0A = true;
               bool   unk0B = true;
            };
            DEFINE_SUBROUTINE(UInt32, Subroutine00AF5820_MaybeLoadModel, 0x00AF5820, const char* path, NiPointer<NiNode>& out, LoadModelOptions& options);
            // another subroutine with the same signature seems to be used for load screens: 0x00AF5680

            constexpr char* ce_filePath = "Dungeons/Dwemer/Animated/DweButton/DweButton01.nif";

            void _dumpNif(NiNode* base, std::string& indent) {
               for (UInt32 i = 0; i < base->m_children.m_emptyRunStart; i++) {
                  auto child = base->m_children.m_data[i];
                  if (child) {
                     _MESSAGE("%s    - Child %d: %s", indent.c_str(), i, child->m_name);
                     auto casted = child->GetAsNiNode();
                     if (casted) {
                        indent += "   ";
                        _dumpNif(casted, indent);
                        indent = indent.substr(0, indent.size() - 3);
                     }
                  } else
                     _MESSAGE("%s    - Child %d: <nullptr>", indent.c_str(), i);
               }
            }
            void RunTest() {
               return; // DISABLE
               

               NiPointer<NiNode> content = nullptr;
               LoadModelOptions options;
               _MESSAGE("About to test loading a model...");
               _MESSAGE("Path: %s", ce_filePath);
               auto eax = Subroutine00AF5820_MaybeLoadModel(ce_filePath, content, options);
               if (eax == 0) {
                  _MESSAGE("Loading seems to have worked.");
                  if (content) {
                     _MESSAGE(" - We have a node! Name is %s.", content->m_name);
                     //
                     // Standard procedure as seen in ActorWeightData::Subroutine00470050 is to create 
                     // a NiCloningProcess, do a little more tampering as needed, and eventually use 
                     // the process to clone the obtained node; you keep the clone and let the original 
                     // die with the smart pointer. Not sure why they do that, but all of the relevant 
                     // code is tangled up in ActorWeightData-specific stuff, and I'm reluctant to do 
                     // a lot of experimentation until I've straightened out what's what.
                     //
                     NiNode* casted = content->GetAsNiNode();
                     if (casted) {
                        std::string indent;
                        _dumpNif(casted, indent);
                     } else {
                        auto rtti = content->GetRTTI();
                        _MESSAGE("It's not a NiNode. RTTI identifies it as: %s", rtti ? rtti->name : "<NO RTTI>");
                     }
                  } else
                     _MESSAGE(" - We didn't get a node...");
               } else {
                  _MESSAGE("Load attempt returned result %08X. :(", eax);
               }
               _MESSAGE("Test complete.");
            }
         }
      }
   }
}