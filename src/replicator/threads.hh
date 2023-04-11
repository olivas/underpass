//
// Copyright (c) 2020, 2021, 2022, 2023 Humanitarian OpenStreetMap Team
//
// This file is part of Underpass.
//
//     Underpass is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Underpass is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Underpass.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef __THREADS_HH__
#define __THREADS_HH__

/// \file threads.hh
/// \brief Threads for monitoring/synchronizing various data sources:
///         - OSM planet server for replication files.
///         - TM users table DB
///
/// These are the threads used to download and apply the replication files
/// to a database. The thread monitors the OSM planet server for updated
/// replication files.
/// Another thread imports users data from TM database.

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <future>
#include <iostream>
#include <pqxx/pqxx>
#include <string>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <ogr_geometry.h>
#include <thread>

#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/version.hpp>
#include <boost/thread/future.hpp>
#include <boost/circular_buffer.hpp>

namespace beast = boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace http = beast::http;
using tcp = net::ip::tcp;

#include "replicator/replication.hh"
#include "underpassconfig.hh"
#include "stats/querystats.hh"
#include "validate/queryvalidate.hh"
#include "validate/validate.hh"
#include <ogr_geometry.h>

using namespace queryvalidate;
using namespace querystats;

namespace replication {
class StateFile;
class RemoteURL;
}; // namespace replication

typedef std::shared_ptr<Validate>(plugin_t)();

/// \namespace threads
namespace threads {

/// \struct ReplicationTask
/// \brief Represents a replication task
struct ReplicationTask {
    std::string url;
    ptime timestamp = not_a_date_time;
    replication::reqfile_t status = replication::reqfile_t::none;
    std::string query = "";
};

/// This monitors the planet server for new changesets files.
/// It does a bulk download to catch up the database, then checks for the
/// minutely change files and processes them.
extern void
startMonitorChangesets(std::shared_ptr<replication::RemoteURL> &remote,
    const multipolygon_t &poly,
    const underpassconfig::UnderpassConfig &config
);

/// This updates several fields in the changesets table, which are part of
/// the changeset file, and don't need to be calculated.
void
threadChangeSet(std::shared_ptr<replication::RemoteURL> &remote,
    std::shared_ptr<replication::Planet> &planet,
    const multipolygon_t &poly,
    std::shared_ptr<std::vector<ReplicationTask>> tasks,
    std::shared_ptr<QueryStats> &querystats
);

/// This monitors the planet server for new OSM changes files.
/// It does a bulk download to catch up the database, then checks for the
/// minutely change files and processes them.
extern void
startMonitorChanges(std::shared_ptr<replication::RemoteURL> &remote,
    const multipolygon_t &poly,
    const underpassconfig::UnderpassConfig &config
);

/// Updates the tables from a changeset file
void threadOsmChange(std::shared_ptr<replication::RemoteURL> &remote,
    std::shared_ptr<replication::Planet> &planet,
    const multipolygon_t &poly,
    std::shared_ptr<Validate> &plugin,
    std::shared_ptr<std::vector<ReplicationTask>> tasks,
    std::shared_ptr<QueryStats> &querystats,
    std::shared_ptr<QueryValidate> &queryvalidate
);

static std::mutex tasks_change_mutex;
static std::mutex tasks_changeset_mutex;

} // namespace threads

#endif // EOF __THREADS_HH__

// local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
