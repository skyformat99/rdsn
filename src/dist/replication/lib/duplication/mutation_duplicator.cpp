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

#include "mutation_duplicator.h"
#include "load_from_private_log.h"
#include "duplication_pipeline.h"

#include <dsn/dist/replication/replication_app_base.h>

namespace dsn {
namespace replication {

mutation_duplicator::mutation_duplicator(const duplication_entry &ent, replica *r)

    : _id(ent.dupid),
      _remote_cluster_address(ent.remote_address),
      _replica(r),
      _view(make_unique<duplication_view>())
{
    auto it = ent.progress.find(get_gpid().get_partition_index());
    if (it != ent.progress.end()) {
        _view->last_decree = _view->confirmed_decree = it->second;
    }
    _view->status = ent.status;

    /// ===== pipeline declaration ===== ///

    thread_pool(LPC_DUPLICATE_MUTATIONS)
        .task_tracker(tracker())
        .thread_hash(get_gpid().thread_hash());

    // load -> ship -> load
    _ship = dsn::make_unique<ship_mutation>(this);
    _load = dsn::make_unique<load_mutation>(this, _replica);

    from(*_load).link(*_ship).link(*_load);
}

void mutation_duplicator::start()
{
    ddebug_replica("starting duplication [dupid: {}, remote: {}]", id(), remote_cluster_address());
    run_pipeline();
}

mutation_duplicator::~mutation_duplicator()
{
    pause();
    wait_all();
}

} // namespace replication
} // namespace dsn
