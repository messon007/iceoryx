// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iceoryx_posh/internal/popo/building_blocks/chunk_queue_data.hpp"
#include "iceoryx_posh/internal/popo/building_blocks/chunk_queue_popper.hpp"
#include "iceoryx_posh/internal/popo/building_blocks/chunk_queue_pusher.hpp"

#include "iceoryx_posh/internal/mepoo/memory_manager.hpp"
#include "iceoryx_posh/internal/mepoo/shared_chunk.hpp"
#include "iceoryx_posh/internal/mepoo/typed_mem_pool.hpp"
#include "iceoryx_posh/mepoo/chunk_header.hpp"
#include "iceoryx_utils/internal/posix_wrapper/shared_memory_object/allocator.hpp"
#include "iceoryx_utils/posix_wrapper/semaphore.hpp"

#include "test.hpp"

using namespace ::testing;
using ::testing::Return;

using namespace iox::popo;
using namespace iox::mepoo;

class ChunkQueue_testBase
{
  public:
    SharedChunk allocateChunk()
    {
        ChunkManagement* chunkMgmt = static_cast<ChunkManagement*>(chunkMgmtPool.getChunk());
        auto chunk = mempool.getChunk();
        ChunkHeader* chunkHeader = new (chunk) ChunkHeader();
        new (chunkMgmt) ChunkManagement{chunkHeader, &mempool, &chunkMgmtPool};
        return SharedChunk(chunkMgmt);
    }

    static constexpr size_t MEGABYTE = 1 << 20;
    static constexpr size_t MEMORY_SIZE = 4 * MEGABYTE;
    std::unique_ptr<char[]> memory{new char[MEMORY_SIZE]};
    iox::posix::Allocator allocator{memory.get(), MEMORY_SIZE};
    MemPool mempool{128, 2 * iox::MAX_RECEIVER_QUEUE_CAPACITY, &allocator, &allocator};
    MemPool chunkMgmtPool{128, 2 * iox::MAX_RECEIVER_QUEUE_CAPACITY, &allocator, &allocator};
    TypedMemPool<iox::posix::Semaphore> semaphorePool{10, &allocator, &allocator};

    static constexpr uint32_t RESIZED_CAPACITY{5u};
};

class ChunkQueue_test : public TestWithParam<iox::cxx::VariantQueueTypes>, public ChunkQueue_testBase
{
  public:
    void SetUp() override{};
    void TearDown() override{};

    ChunkQueueData m_chunkData{GetParam()};
    ChunkQueuePopper m_popper{&m_chunkData};
    ChunkQueuePusher m_pusher{&m_chunkData};
};

/// we require INSTANTIATE_TEST_CASE since we support gtest 1.8 for our safety targets
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
INSTANTIATE_TEST_CASE_P(ChunkQueueAll,
                        ChunkQueue_test,
                        Values(iox::cxx::VariantQueueTypes::FiFo_SingleProducerSingleConsumer,
                               iox::cxx::VariantQueueTypes::SoFi_SingleProducerSingleConsumer));
#pragma GCC diagnostic pop

TEST_P(ChunkQueue_test, InitialEmpty)
{
    EXPECT_THAT(m_popper.empty(), Eq(true));
}

TEST_P(ChunkQueue_test, InitialSemaphoreAttached)
{
    EXPECT_THAT(m_popper.isSemaphoreAttached(), Eq(false));
}

TEST_P(ChunkQueue_test, PushOneChunk)
{
    auto chunk = allocateChunk();
    auto ret = m_pusher.push(chunk);
    EXPECT_FALSE(ret.has_error());
    EXPECT_THAT(m_popper.empty(), Eq(false));
    /// @note size not implemented on FIFO
    if (GetParam() != iox::cxx::VariantQueueTypes::FiFo_SingleProducerSingleConsumer)
    {
        EXPECT_THAT(m_popper.size(), Eq(1u));
    }
}

TEST_P(ChunkQueue_test, PopOneChunk)
{
    auto chunk = allocateChunk();
    m_pusher.push(chunk);

    EXPECT_THAT(m_popper.pop().has_value(), Eq(true));
    EXPECT_THAT(m_popper.empty(), Eq(true));
    /// @note size not implemented on FIFO
    if (GetParam() != iox::cxx::VariantQueueTypes::FiFo_SingleProducerSingleConsumer)
    {
        EXPECT_THAT(m_popper.size(), Eq(0u));
    }
}

TEST_P(ChunkQueue_test, PushedChunksMustBePoppedInTheSameOrder)
{
    constexpr int32_t NUMBER_CHUNKS{5};
    for (int i = 0; i < NUMBER_CHUNKS; ++i)
    {
        auto chunk = allocateChunk();
        *reinterpret_cast<int32_t*>(chunk.getPayload()) = i;
        m_pusher.push(chunk);
    }

    for (int i = 0; i < NUMBER_CHUNKS; ++i)
    {
        auto maybeSharedChunk = m_popper.pop();
        ASSERT_THAT(maybeSharedChunk.has_value(), Eq(true));
        auto data = *reinterpret_cast<int32_t*>((*maybeSharedChunk).getPayload());
        EXPECT_THAT(data, Eq(i));
    }
}

TEST_P(ChunkQueue_test, ClearOnEmpty)
{
    m_popper.clear();
    EXPECT_THAT(m_popper.empty(), Eq(true));
}

TEST_P(ChunkQueue_test, ClearWithData)
{
    auto chunk = allocateChunk();
    m_pusher.push(chunk);
    m_popper.clear();
    EXPECT_THAT(m_popper.empty(), Eq(true));
}

TEST_P(ChunkQueue_test, AttachSemaphore)
{
    auto semaphore = semaphorePool.createObjectWithCreationPattern<iox::posix::Semaphore::errorType_t>(0);
    ASSERT_THAT(semaphore.has_error(), Eq(false));

    auto ret = m_popper.attachSemaphore(*semaphore);
    EXPECT_FALSE(ret.has_error());

    EXPECT_THAT(m_popper.isSemaphoreAttached(), Eq(true));
}

TEST_P(ChunkQueue_test, DISABLED_PushAndTriggersSemaphore)
{
    auto semaphore = semaphorePool.createObjectWithCreationPattern<iox::posix::Semaphore::errorType_t>(0);
    ASSERT_THAT(semaphore.has_error(), Eq(false));

    auto ret = m_popper.attachSemaphore(*semaphore);
    EXPECT_FALSE(ret.has_error());

    EXPECT_THAT(semaphore->get()->tryWait(), Eq(false));

    auto chunk = allocateChunk();
    m_pusher.push(chunk);

    EXPECT_THAT(semaphore->get()->tryWait(), Eq(true));
    EXPECT_THAT(semaphore->get()->tryWait(), Eq(false)); // shouldn't trigger a second time
}

TEST_P(ChunkQueue_test, DISABLED_AttachSecondSemaphore)
{
    auto semaphore1 = semaphorePool.createObjectWithCreationPattern<iox::posix::Semaphore::errorType_t>(0);
    ASSERT_THAT(semaphore1.has_error(), Eq(false));
    auto semaphore2 = semaphorePool.createObjectWithCreationPattern<iox::posix::Semaphore::errorType_t>(0);
    ASSERT_THAT(semaphore2.has_error(), Eq(false));

    auto ret1 = m_popper.attachSemaphore(*semaphore1);
    EXPECT_FALSE(ret1.has_error());

    auto ret2 = m_popper.attachSemaphore(*semaphore2);
    EXPECT_TRUE(ret2.has_error());
    ASSERT_THAT(ret2.get_error(), Eq(ChunkQueueError::SEMAPHORE_ALREADY_SET));

    EXPECT_THAT(semaphore1->get()->tryWait(), Eq(false));
    EXPECT_THAT(semaphore2->get()->tryWait(), Eq(false));

    auto chunk = allocateChunk();
    m_pusher.push(chunk);

    EXPECT_THAT(semaphore1->get()->tryWait(), Eq(true));
    EXPECT_THAT(semaphore2->get()->tryWait(), Eq(false));
}

/// @note this could be changed to a parameterized ChunkQueueSaturatingFIFO_test when there are more FIFOs available
class ChunkQueueFiFo_test : public Test, public ChunkQueue_testBase
{
  public:
    void SetUp() override{};
    void TearDown() override{};

    ChunkQueueData m_chunkData{iox::cxx::VariantQueueTypes::FiFo_SingleProducerSingleConsumer};
    ChunkQueuePopper m_popper{&m_chunkData};
    ChunkQueuePusher m_pusher{&m_chunkData};
};

/// @note API currently not supported
TEST_F(ChunkQueueFiFo_test, DISABLED_InitialSize)
{
    EXPECT_THAT(m_popper.size(), Eq(0u));
}

/// @note API currently not supported
TEST_F(ChunkQueueFiFo_test, DISABLED_Capacity)
{
    EXPECT_THAT(m_popper.getCurrentCapacity(), Eq(iox::MAX_RECEIVER_QUEUE_CAPACITY));
}


/// @note API currently not supported
TEST_F(ChunkQueueFiFo_test, DISABLED_SetCapacity)
{
    m_popper.setCapacity(RESIZED_CAPACITY);
    EXPECT_THAT(m_popper.getCurrentCapacity(), Eq(RESIZED_CAPACITY));
}

TEST_F(ChunkQueueFiFo_test, PushFull)
{
    for (auto i = 0u; i < iox::MAX_RECEIVER_QUEUE_CAPACITY; ++i)
    {
        auto chunk = allocateChunk();
        m_pusher.push(chunk);
    }
    auto chunk = allocateChunk();
    auto ret = m_pusher.push(chunk);
    EXPECT_TRUE(ret.has_error());
    EXPECT_THAT(ret.get_error(), Eq(iox::popo::ChunkQueueError::QUEUE_OVERFLOW));
    EXPECT_THAT(m_popper.empty(), Eq(false));
}

/// @note this could be changed to a parameterized ChunkQueueOverflowingFIFO_test when there are more FIFOs available
class ChunkQueueSoFi_test : public Test, public ChunkQueue_testBase
{
  public:
    void SetUp() override{};
    void TearDown() override{};

    ChunkQueueData m_chunkData{iox::cxx::VariantQueueTypes::SoFi_SingleProducerSingleConsumer};
    ChunkQueuePopper m_popper{&m_chunkData};
    ChunkQueuePusher m_pusher{&m_chunkData};
};

TEST_F(ChunkQueueSoFi_test, InitialSize)
{
    EXPECT_THAT(m_popper.size(), Eq(0u));
}

TEST_F(ChunkQueueSoFi_test, Capacity)
{
    EXPECT_THAT(m_popper.getCurrentCapacity(), Eq(iox::MAX_RECEIVER_QUEUE_CAPACITY));
}


TEST_F(ChunkQueueSoFi_test, SetCapacity)
{
    m_popper.setCapacity(RESIZED_CAPACITY);
    EXPECT_THAT(m_popper.getCurrentCapacity(), Eq(RESIZED_CAPACITY));
}

TEST_F(ChunkQueueSoFi_test, PushFull)
{
    for (auto i = 0u; i < iox::MAX_RECEIVER_QUEUE_CAPACITY * 2; ++i)
    {
        auto chunk = allocateChunk();
        m_pusher.push(chunk);
    }

    {
        // pushing is still fine
        auto chunk = allocateChunk();
        auto ret = m_pusher.push(chunk);
        EXPECT_FALSE(ret.has_error());
        EXPECT_THAT(m_popper.empty(), Eq(false));
    }
    // get al the chunks in the queue
    while (m_popper.pop().has_value())
    {
    }

    // now all chunks are released
    EXPECT_THAT(mempool.getUsedChunks(), Eq(0u));
}
