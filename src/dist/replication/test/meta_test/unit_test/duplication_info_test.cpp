/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "dist/replication/meta_server/duplication/duplication_info.h"

#include <gtest/gtest.h>

namespace dsn {
namespace replication {

class duplication_info_test : public testing::Test
{
public:
    void pass_progress_time_wait(duplication_info *dup)
    {
        dup->last_progress_update -= duplication_info::PROGRESS_UPDATE_PERIOD_MS + 100;
    }
};

TEST_F(duplication_info_test, init_and_start)
{
    duplication_info dup(1, 1, "dsn://slave-cluster/temp", "/meta_test/101/duplication/1");
    ASSERT_FALSE(dup.is_altering());
    ASSERT_EQ(dup.status, duplication_status::DS_INIT);
    ASSERT_EQ(dup.next_status, duplication_status::DS_INIT);

    dup.start();
    ASSERT_TRUE(dup.is_altering());
    ASSERT_EQ(dup.status, duplication_status::DS_INIT);
    ASSERT_EQ(dup.next_status, duplication_status::DS_START);
}

TEST_F(duplication_info_test, stable_status)
{
    duplication_info dup(1, 1, "dsn://slave-cluster/temp", "/meta_test/101/duplication/1");
    dup.start();

    dup.stable_status();
    ASSERT_EQ(dup.status, duplication_status::DS_START);
    ASSERT_EQ(dup.next_status, duplication_status::DS_INIT);
    ASSERT_FALSE(dup.is_altering());
}

TEST_F(duplication_info_test, alter_status_when_busy)
{
    duplication_info dup(1, 1, "dsn://slave-cluster/temp", "/meta_test/101/duplication/1");
    dup.start();

    ASSERT_EQ(dup.alter_status(duplication_status::DS_PAUSE), ERR_BUSY);
}

TEST_F(duplication_info_test, alter_status)
{
    struct TestData
    {
        duplication_status::type from;
        duplication_status::type to;

        error_code wec;
    } tests[] = {
        {duplication_status::DS_PAUSE, duplication_status::DS_PAUSE, ERR_OK},
        {duplication_status::DS_PAUSE, duplication_status::DS_START, ERR_OK},
        {duplication_status::DS_PAUSE, duplication_status::DS_INIT, ERR_INVALID_PARAMETERS},
        {duplication_status::DS_PAUSE, duplication_status::DS_REMOVED, ERR_OK},
        {duplication_status::DS_START, duplication_status::DS_START, ERR_OK},
        {duplication_status::DS_START, duplication_status::DS_PAUSE, ERR_OK},
        {duplication_status::DS_START, duplication_status::DS_REMOVED, ERR_OK},
        {duplication_status::DS_START, duplication_status::DS_INIT, ERR_INVALID_PARAMETERS},

        // alter unavail dup
        {duplication_status::DS_REMOVED, duplication_status::DS_INIT, ERR_OBJECT_NOT_FOUND},
        {duplication_status::DS_REMOVED, duplication_status::DS_PAUSE, ERR_OBJECT_NOT_FOUND},
        {duplication_status::DS_REMOVED, duplication_status::DS_START, ERR_OBJECT_NOT_FOUND},
    };

    for (auto tt : tests) {
        duplication_info dup(1, 1, "dsn://slave-cluster/temp", "/meta_test/101/duplication/1");
        dup.start();
        dup.stable_status();

        ASSERT_EQ(dup.alter_status(tt.from), ERR_OK);
        if (dup.is_altering()) {
            dup.stable_status();
        }

        ASSERT_EQ(dup.alter_status(tt.to), tt.wec);
    }
}

TEST_F(duplication_info_test, encode_and_decode)
{
    duplication_info dup(1, 1, "dsn://slave-cluster/temp", "/meta_test/101/duplication/1");
    dup.start();
    dup.stable_status();

    auto copy_dup = dup.copy();
    ASSERT_EQ(copy_dup->to_string(), dup.to_string());
}

TEST_F(duplication_info_test, alter_progress)
{
    duplication_info dup(1, 1, "dsn://slave-cluster/temp", "/meta_test/101/duplication/1");

    dup.alter_progress(1, 5);
    ASSERT_EQ(dup.progress[1], 5);
    ASSERT_TRUE(dup.is_altering());

    // busy updating
    ASSERT_FALSE(dup.alter_progress(1, 10));
    ASSERT_EQ(dup.progress[1], 5);
    ASSERT_TRUE(dup.is_altering());

    dup.stable_progress();
    ASSERT_EQ(dup.stored_progress[1], 5);
    ASSERT_FALSE(dup.is_altering());

    // too frequent to update
    ASSERT_FALSE(dup.alter_progress(1, 10));
    ASSERT_FALSE(dup.is_altering());

    pass_progress_time_wait(&dup);
    ASSERT_TRUE(dup.alter_progress(1, 15));
    ASSERT_TRUE(dup.is_altering());
}

} // namespace replication
} // namespace dsn