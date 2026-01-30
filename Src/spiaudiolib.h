/* Copyright 2025 Aidan Holmes

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef __DEVTREELIBRARY_H
#define __DEVTREELIBRARY_H

#include <exec/types.h>
#include "libdev.h"
#include "compatibility.h"


__SAVE_DS__ struct LibDevBase* __ASM__ libdev_library_open(__REG__(a6, struct LibDevBase *) base);
__SAVE_DS__ struct LibDevBase* __ASM__ libdev_initalise(__REG__(a6, struct LibDevBase *) base) ;
__SAVE_DS__ void __ASM__ libdev_cleanup(__REG__(a6, struct LibDevBase *) base);


#endif