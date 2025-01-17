// Copyright (c) 2019 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 - 2022 by Apex.AI Inc. All rights reserved.
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
//
// SPDX-License-Identifier: Apache-2.0

#include "iceoryx_hoofs/log/logging.hpp"

#include "iceoryx_hoofs/testing/mocks/logger_mock.hpp"
#include "iceoryx_hoofs/testing/testing_logger.hpp"
#include "test.hpp"

namespace
{
using namespace ::testing;

void testLogLevelThreshold(const iox::log::LogLevel loggerLogLevel)
{
    iox::log::Logger::setLogLevel(loggerLogLevel);

    struct LogLevel
    {
        LogLevel(iox::log::LogLevel logLevel, std::string str)
            : value(logLevel)
            , string(std::move(str))
        {
        }
        iox::log::LogLevel value;
        std::string string;
    };

    const std::initializer_list<LogLevel> logEntryLogLevels{{iox::log::LogLevel::FATAL, "Fatal"},
                                                            {iox::log::LogLevel::ERROR, "Error"},
                                                            {iox::log::LogLevel::WARN, "Warn"},
                                                            {iox::log::LogLevel::INFO, "Info"},
                                                            {iox::log::LogLevel::DEBUG, "Debug"},
                                                            {iox::log::LogLevel::TRACE, "Trace"}};

    iox::testing::Logger_Mock loggerMock;
    for (const auto& logEntryLogLevel : logEntryLogLevels)
    {
        if (!iox::testing::TestingLogger::doesLoggerSupportLogLevel(logEntryLogLevel.value))
        {
            continue;
        }

        dynamic_cast<iox::testing::TestingLogger&>(iox::log::Logger::get()).clearLogBuffer();
        IOX_LOG_INTERNAL("", 0, "", logEntryLogLevel.value);

        if (logEntryLogLevel.value <= loggerLogLevel)
        {
            ASSERT_THAT(iox::testing::TestingLogger::getNumberOfLogMessages(), Eq(1U));
            auto logMessage = iox::testing::TestingLogger::getLogMessages().back();
            EXPECT_THAT(logMessage.find(logEntryLogLevel.string), Ne(std::string::npos));
        }
        else
        {
            ASSERT_THAT(iox::testing::TestingLogger::getNumberOfLogMessages(), Eq(0U));
        }
    }
}

TEST(LoggingLogLevelThreshold_test, LogLevel)
{
    ::testing::Test::RecordProperty("TEST_ID", "829a6634-43be-4fa4-94bf-18d53ce816a9");
    for (const auto loggerLogLevel : {iox::log::LogLevel::OFF,
                                      iox::log::LogLevel::FATAL,
                                      iox::log::LogLevel::ERROR,
                                      iox::log::LogLevel::WARN,
                                      iox::log::LogLevel::INFO,
                                      iox::log::LogLevel::DEBUG,
                                      iox::log::LogLevel::TRACE})
    {
        SCOPED_TRACE(std::string("Logger LogLevel: ") + iox::log::asStringLiteral(loggerLogLevel));

        testLogLevelThreshold(loggerLogLevel);
    }
}

} // namespace
