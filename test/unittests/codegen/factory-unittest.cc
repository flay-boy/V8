// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8-isolate.h"
#include "src/codegen/code-desc.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using FactoryCodeBuilderTest = TestWithIsolate;

TEST_F(FactoryCodeBuilderTest, Factory_CodeBuilder) {
  // Create a big function that ends up in CODE_LO_SPACE.
  const int instruction_size =
      i_isolate()->heap()->MaxRegularHeapObjectSize(AllocationType::kCode) + 1;
  std::unique_ptr<byte[]> instructions(new byte[instruction_size]);

  CodeDesc desc;
  desc.buffer = instructions.get();
  desc.buffer_size = instruction_size;
  desc.instr_size = instruction_size;
  desc.reloc_size = 0;
  desc.constant_pool_size = 0;
  desc.unwinding_info = nullptr;
  desc.unwinding_info_size = 0;
  desc.origin = nullptr;
  Handle<Code> code =
      Factory::CodeBuilder(i_isolate(), desc, CodeKind::WASM_FUNCTION).Build();

  CHECK(i_isolate()->heap()->InSpace(*code, CODE_LO_SPACE));
#if VERIFY_HEAP
  code->ObjectVerify(i_isolate());
#endif
}

// This needs to be large enough to create a new nosnap Isolate, but smaller
// than kMaximalCodeRangeSize so we can recover from the OOM.
constexpr int kInstructionSize = 100 * MB;
static_assert(kInstructionSize < kMaximalCodeRangeSize ||
              !kPlatformRequiresCodeRange);

size_t NearHeapLimitCallback(void* raw_bool, size_t current_heap_limit,
                             size_t initial_heap_limit) {
  bool* oom_triggered = static_cast<bool*>(raw_bool);
  *oom_triggered = true;
  return kInstructionSize * 2;
}

class FactoryCodeBuilderOOMTest : public TestWithIsolate {
 public:
  static void SetUpTestSuite() {
    FLAG_max_old_space_size = kInstructionSize / MB / 2;  // In MB.
  }

  void SetUp() override {
    isolate()->heap()->AddNearHeapLimitCallback(NearHeapLimitCallback,
                                                &oom_triggered_);
  }

  bool oom_triggered() const { return oom_triggered_; }

 private:
  bool oom_triggered_ = false;
};

TEST_F(FactoryCodeBuilderOOMTest, Factory_CodeBuilder_BuildOOM) {
  std::unique_ptr<byte[]> instructions(new byte[kInstructionSize]);
  CodeDesc desc;
  desc.instr_size = kInstructionSize;
  desc.buffer = instructions.get();

  const Handle<Code> code =
      Factory::CodeBuilder(i_isolate(), desc, CodeKind::WASM_FUNCTION).Build();

  CHECK(!code.is_null());
  CHECK(oom_triggered());
}

TEST_F(FactoryCodeBuilderOOMTest, Factory_CodeBuilder_TryBuildOOM) {
  std::unique_ptr<byte[]> instructions(new byte[kInstructionSize]);
  CodeDesc desc;
  desc.instr_size = kInstructionSize;
  desc.buffer = instructions.get();

  const MaybeHandle<Code> code =
      Factory::CodeBuilder(i_isolate(), desc, CodeKind::WASM_FUNCTION)
          .TryBuild();

  CHECK(code.is_null());
  CHECK(!oom_triggered());
}

}  // namespace internal
}  // namespace v8
