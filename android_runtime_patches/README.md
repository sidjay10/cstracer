## Patches for Tracing Android Apps using Valgrind

To use cstracer, you need to run an engineering build of android with the following patches applied.

For instructions on building android, visit [source.android.com](https://source.android.com/setup/build/building)


## These patches need to be applied to the android runtime to trace apps

### 1: [art_disable_stack_checks.patch](art_disable_stack_checks.patch)

This patch disables the stack overflow checks that the android runtime adds.
Tracing with valgrind requires that these checks be disabled so that the
application can run in the valgrind environment.

Apply this patch in the art/ repository under source.

### 2: [framework_base_timeout.patch](framework_base_timeout.patch)

This patch increases the default timeouts to prevent application not responding (ANR) timeouts 
from killing the application while tracing. This is required since tracing introduces significant 
slowdowns.

Apply this patch in the frameworks/base repository under source.

### 3: [art_use_art_allocator.patch](art_use_art_allocator.patch)

This patch modifies the runtime to use the art allocator when running on x86_64. Using the MAP_32BIT flag causes
errors while running with Valgrind, so it is disables.

Apply this patch in the art/ repository under source.

### 4: [art_add_redzones.patch](art_add_redzones.patch)

This patch ensures that the allocator provides redzones since valgrind requires the use of redzones.

Apply this patch in the art/ repository under source.
