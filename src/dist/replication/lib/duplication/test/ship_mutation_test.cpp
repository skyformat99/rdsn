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

#include "dist/replication/lib/duplication/mutation_batch.h"
#include "dist/replication/lib/duplication/duplication_pipeline.h"

#include "duplication_test_base.h"

namespace dsn {
namespace replication {

/*static*/ mock_duplication_backlog_handler::duplicate_function
    mock_duplication_backlog_handler::_func;

struct mock_stage : pipeline::when<>
{
    void run() override {}
};

struct ship_mutation_test : public mutation_duplicator_test_base
{
    ship_mutation_test() : duplicator(create_test_duplicator()) {}

    // ensure ship_mutation retries after error.
    // ensure it clears up all pending mutations after stage ends.
    // ensure it update duplicator->last_decree after stage ends.
    void test_ship_mutation_tuple_set()
    {
        ship_mutation shipper(duplicator.get());
        mock_stage end;

        pipeline::base base;
        base.thread_pool(LPC_DUPLICATION_LOAD_MUTATIONS)
            .task_tracker(replica.get())
            .from(shipper)
            .link(end);

        mutation_tuple result;
        bool error_flag = true;
        mock_duplication_backlog_handler::mock([&result, &error_flag](
            mutation_tuple mut, duplication_backlog_handler::err_callback cb) {
            if (error_flag) {
                result = mut;
            } else {
                ASSERT_EQ(std::get<0>(result), std::get<0>(mut));
                ASSERT_EQ(std::get<1>(result), std::get<1>(mut));
                ASSERT_EQ(std::get<2>(result).to_string(), std::get<2>(mut).to_string());
                ASSERT_EQ(std::get<2>(result).to_string(), "hello");
            }
            error_flag = !error_flag;

            if (error_flag) {
                cb(error_s::make(ERR_TIMEOUT));
            } else {
                cb(error_s::make(ERR_OK));
            }
        });

        mutation_batch batch;
        batch.add(create_test_mutation(1, "hello"));
        batch.add(create_test_mutation(2, "hello"));
        shipper.run(2, batch.move_all_mutations());

        base.wait_all();
        ASSERT_EQ(shipper._pending.size(), 0);
        ASSERT_EQ(duplicator->view().last_decree, 2);
    }

    std::unique_ptr<mutation_duplicator> duplicator;
};

TEST_F(ship_mutation_test, ship_mutation_tuple_set) { test_ship_mutation_tuple_set(); }

} // namespace replication
} // namespace dsn