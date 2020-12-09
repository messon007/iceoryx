// Copyright (c) 2020 by Apex.AI Inc. All rights reserved.
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

#include "iceoryx_posh/popo/trigger.hpp"
#include "iceoryx_posh/internal/popo/building_blocks/condition_variable_signaler.hpp"

namespace iox
{
namespace popo
{
std::atomic<uint64_t> Trigger::uniqueIdCounter{0U};

Trigger::~Trigger()
{
    reset();
}

bool Trigger::hasTriggered() const noexcept
{
    return (isValid()) ? m_hasTriggeredCallback().value() : false;
}

void Trigger::reset() noexcept
{
    if (!isValid())
    {
        return;
    }

    if (m_resetCallback)
    {
        m_resetCallback(m_uniqueId);
    }

    invalidate();
}

const TriggerInfo& Trigger::getTriggerInfo() const noexcept
{
    return m_triggerInfo;
}

void Trigger::invalidate() noexcept
{
    m_hasTriggeredCallback = cxx::ConstMethodCallback<bool>();
}

Trigger::operator bool() const noexcept
{
    return isValid();
}

bool Trigger::isValid() const noexcept
{
    return static_cast<bool>(m_hasTriggeredCallback);
}

bool Trigger::isLogicalEqualTo(const Trigger& rhs) const noexcept
{
    return (isValid() && rhs.isValid() && m_triggerInfo.m_triggerOrigin == rhs.m_triggerInfo.m_triggerOrigin
            && m_hasTriggeredCallback == rhs.m_hasTriggeredCallback
            && m_triggerInfo.m_triggerId == rhs.m_triggerInfo.m_triggerId);
}

Trigger::Trigger(Trigger&& rhs) noexcept
{
    *this = std::move(rhs);
}

Trigger& Trigger::operator=(Trigger&& rhs) noexcept
{
    if (this != &rhs)
    {
        reset();

        // TriggerInfo
        m_triggerInfo = std::move(rhs.m_triggerInfo);

        // Trigger
        m_resetCallback = rhs.m_resetCallback;
        m_hasTriggeredCallback = rhs.m_hasTriggeredCallback;
        m_uniqueId = rhs.m_uniqueId;

        rhs.invalidate();
    }
    return *this;
}

uint64_t Trigger::getUniqueId() const noexcept
{
    return m_uniqueId;
}


} // namespace popo
} // namespace iox