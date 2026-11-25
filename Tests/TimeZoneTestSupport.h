/*
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <gtest/gtest.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

class TimeZoneTestFiles {
public:
    const char* const timeZone = "America/New_York";
    const char* const zoneFile = "/usr/share/zoneinfo/America/New_York";
    const char* const localtime = "/opt/persistent/localtime";

    TimeZoneTestFiles() = default;
    TimeZoneTestFiles(const TimeZoneTestFiles&) = delete;
    TimeZoneTestFiles& operator=(const TimeZoneTestFiles&) = delete;

    void SetUp()
    {
        ASSERT_EQ(0, access(zoneFile, R_OK)) << strerror(errno);
        char directory[] = "/opt/persistent/systemservices-timezone-XXXXXX";
        ASSERT_NE(nullptr, mkdtemp(directory)) << strerror(errno);
        backupDirectory = directory;

        // Rename rather than copy so existing symlinks and file metadata survive.
        for (auto& file : files) {
            const std::string original = std::string("/opt/persistent/") + file.name;
            const std::string backup = backupDirectory + "/" + file.name;
            if (rename(original.c_str(), backup.c_str()) == 0) {
                file.saved = true;
            } else {
                ASSERT_EQ(ENOENT, errno) << original << ": " << strerror(errno);
            }
            file.prepared = true;
        }

        std::ofstream timezoneFile("/opt/persistent/timeZoneDST");
        timezoneFile << timeZone;
        timezoneFile.close();
        ASSERT_TRUE(timezoneFile.good());
    }

    ~TimeZoneTestFiles()
    {
        if (backupDirectory.empty()) {
            return;
        }
        for (const auto& file : files) {
            if (!file.prepared) {
                continue;
            }
            const std::string original = std::string("/opt/persistent/") + file.name;
            if (remove(original.c_str()) != 0) {
                EXPECT_EQ(ENOENT, errno) << original << ": " << strerror(errno);
            }
            if (file.saved) {
                const std::string backup = backupDirectory + "/" + file.name;
                EXPECT_EQ(0, rename(backup.c_str(), original.c_str())) << strerror(errno);
            }
        }
        EXPECT_EQ(0, rmdir(backupDirectory.c_str())) << strerror(errno);
    }

    void ExpectLinkTarget(const std::string& expected) const
    {
        char target[4096];
        const ssize_t length = readlink(localtime, target, sizeof(target));
        ASSERT_GE(length, 0) << strerror(errno);
        EXPECT_EQ(expected, std::string(target, static_cast<size_t>(length)));
    }

    void ExpectUnchangedSettings() const
    {
        std::ifstream timezoneFile("/opt/persistent/timeZoneDST");
        std::string value;
        ASSERT_TRUE(static_cast<bool>(std::getline(timezoneFile, value)));
        EXPECT_EQ(timeZone, value);
    }

    std::string MissingTarget() const
    {
        return backupDirectory + "/missing-zone";
    }

private:
    struct SavedFile {
        const char* name;
        bool saved;
        bool prepared;
    };
    SavedFile files[3] = {
        {"timeZoneDST", false, false},
        {"timeZoneDSTAccuracy", false, false},
        {"localtime", false, false}
    };
    std::string backupDirectory;
};

}
