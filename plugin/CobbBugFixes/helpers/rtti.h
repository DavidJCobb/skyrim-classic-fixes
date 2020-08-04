/*

This file is provided under the Creative Commons 0 License.
License: <https://creativecommons.org/publicdomain/zero/1.0/legalcode>
Summary: <https://creativecommons.org/publicdomain/zero/1.0/>

One-line summary: This file is public domain or the closest legal equivalent.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
#pragma once
#include <cstdint>

namespace cobb {
   namespace rtti {
      struct class_hierarchy_descriptor;
      class type_descriptor;
      struct base_class;

      struct complete_object_locator {
         uint32_t signature = 0; // 00
         uint32_t v_offset;      // 04 // offset of this vtbl in the complete class
         uint32_t c_offset;      // 08 // offset used for the constructor, i.e. *(type*)(completeClass + c_offset)->constructor();
         type_descriptor* typeinfo; // 0C
         class_hierarchy_descriptor* hierarchy; // 10
         //
         inline static complete_object_locator& get_from_virtual_object(void* object) {
            auto vtbl = *(uint32_t*)object;
            return get_from_vtbl(vtbl);
         }
         inline static complete_object_locator& get_from_vtbl(uint32_t vtbl) {
            auto col = *(uint32_t*)(vtbl - sizeof(void*));
            return *(complete_object_locator*)col;
         }
      };
      struct class_hierarchy_descriptor {
         uint32_t     signature = 0;
         uint32_t     attributes;
         uint32_t     base_class_count;
         base_class** base_classes; // NOTE: the first entry in a type's base classes array may be the type itself. for example, in TESV.exe, TESObjectACTI is the first entry in TESObjectACTI's base class list
         //
         inline bool uses_multiple_inheritance() const noexcept { return this->attributes & 1; }
         inline bool uses_virtual_inheritance()  const noexcept { return this->attributes & 2; }
      };
      struct base_class {
         struct _pmd { // pointer-to-member displacement
            int32_t member;
            int32_t vtbl = -1; // sentinel value -1 is used to indicate "don't use me"
            int32_t inside_vtbl;
         };
         //
         type_descriptor* typeinfo; // 00
         uint32_t contained_base_count; // 04 // the next (contained_base_count) entries in the base-classes array are nested under this one
         _pmd     pmd; // 08
         uint32_t attributes; // 14
         //
         void* base_from_derived(void* derived) { // base* + (return value) = derived*
            char* p = (char*)derived + this->pmd.member;
            if (this->pmd.vtbl != -1) {
               char* vbtable = p + this->pmd.vtbl;
               p += *(int32_t*)(vbtable + pmd.inside_vtbl);
            }
            return (void*)p;
         }
      };
      class type_descriptor { // this is std::type_info
         protected:
            virtual ~type_descriptor();
            //
            uint32_t unk04 = 0; // 04
            char     _name;     // 08 // it's built into the class, but its length varies, which also means that the size of any particular type_descriptor varies
            //
         public:
            const char* name() const noexcept { return &this->_name; }
      };
   }
}