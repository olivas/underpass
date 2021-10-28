//
// Copyright (c) 2020, 2021 Humanitarian OpenStreetMap Team
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

#ifndef __REPLICATOR_CONFIG_HH__
#define __REPLICATOR_CONFIG_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include "data/underpass.hh"
#include "osmstats/replication.hh"
#include <boost/format.hpp>
#include <string>
using namespace replication;
using namespace underpass;

namespace replicatorconfig {

///
/// \brief The PlanetServer struct represents a planet server
///
struct PlanetServer {

    ///
    /// \brief PlanetServer
    /// \param domin domain part (without https://)
    /// \param datadir (usually "replication")
    /// \param daily supports daily
    /// \param hourly supports hourly
    /// \param minutely supports minutely
    /// \param changeset supports changeset
    ///
    PlanetServer(const std::string &url, const std::string &datadir, bool daily, bool hourly, bool minutely, bool changeset)
        : domain(url), datadir(datadir), has_daily(daily), has_hourly(hourly), has_minutely(minutely), has_changeset(changeset)
    {
    }

    std::string domain;
    std::string datadir;
    bool has_daily = false;
    bool has_hourly = false;
    bool has_minutely = false;
    bool has_changeset = false;

    ///
    /// \brief has_frequency returns TRUE if the \a frequency is supported by the server.
    ///
    bool has_frequency(frequency_t frequency) const
    {
        switch (frequency) {
            case frequency_t::daily:
                return has_daily;
            case frequency_t::hourly:
                return has_hourly;
            case frequency_t::minutely:
                return has_minutely;
            case frequency_t::changeset:
                return has_changeset;
        }
        return false;
    }

    ///
    /// \brief baseUrl returns the full base url including the datadir
    ///        (e.g. "https://free.nchc.org.tw/osm.planet/replcation" or "https://download.openstreetmap.fr/replication")
    ///
    std::string replicationUrl() const
    {
        return "https://" + domain + (datadir.empty() ? "" : "/" + datadir);
    }
};

///
/// \brief The ReplicatorConfig struct stores replicator configuration
///
struct ReplicatorConfig {

    ///
    /// \brief ReplicatorConfig constructor: will try to initialize from uppercased same-name
    ///        environment variables prefixed by REPLICATOR_ (e.g. REPLICATOR_OSMSTATS_DB_URL)
    ///
    ReplicatorConfig()
    {
        if (getenv("REPLICATOR_OSMSTATS_DB_URL")) {
            osmstats_db_url = getenv("REPLICATOR_OSMSTATS_DB_URL");
        }
        if (getenv("REPLICATOR_TASKINGMANAGER_DB_URL")) {
            taskingmanager_db_url = getenv("REPLICATOR_TASKINGMANAGER_DB_URL");
        }
        if (getenv("REPLICATOR_UNDERPASS_DB_URL")) {
            underpass_db_url = getenv("REPLICATOR_UNDERPASS_DB_URL");
        }
        if (getenv("REPLICATOR_OSM2PGSQL_DB_URL")) {
            osm2pgsql_db_url = getenv("REPLICATOR__OSM2PGSQL_DB_URL");
        }
        if (getenv("REPLICATOR_PLANET_SERVER")) {
            planet_server = getenv("REPLICATOR_PLANET_SERVER");
        }
        if (getenv("REPLICATOR_FREQUENCY")) {
            try {
                frequency = Underpass::freq_from_string(getenv("REPLICATOR_FREQUENCY"));
            } catch (const std::invalid_argument &) {
                // Ignore
            }
        }
        if (getenv("REPLICATOR_TASKINGMANAGER_USERS_UPDATE_FREQUENCY")) {
            try {
                const auto tm_freq{std::atoi(getenv("REPLICATOR_TASKINGMANAGER_USERS_UPDATE_FREQUENCY"))};
                if (tm_freq >= -1) {
                    taskingmanager_users_update_frequency = tm_freq;
                }
            } catch (const std::exception &) {
                // Ignore
            }
        }

        // Initialize servers
        planet_servers = {
            {"planet.maps.mail.ru", "replication", true, true, true, true},
            {"download.openstreetmap.fr", "replication", false, false, true, false},
            // This may be too slow
            {"planet.openstreetmap.org", "replication", true, true, true, true},
            // This is not up to date (one day late, I'm keeping here for debugging purposes because it fails on updates):
            // {"free.nchc.org.tw", "osm.planet/replication", true, true, true, true},
        };
    };

    std::string underpass_db_url = "localhost/underpass";
    std::string osmstats_db_url = "localhost/osmstats";
    std::string taskingmanager_db_url = "localhost/taskingmanager";
    std::string osm2pgsql_db_url = "";
    std::string planet_server;
    std::string datadir;
    std::vector<PlanetServer> planet_servers;
    unsigned int concurrency = 1;

    frequency_t frequency = frequency_t::minutely;
    ptime start_time = not_a_date_time;              ///< Starting time for changesets and OSM changes import
    ptime end_time = not_a_date_time;                ///< Ending time for changesets and OSM changes import
    long taskingmanager_users_update_frequency = -1; ///< Users synchronization: -1 (disabled), 0 (single shot), > 0 (interval in seconds)

    ///
    /// \brief getPlanetServer returns either the command line supplied planet server
    ///        replication URL or the first planet server replication URL from the hardcoded
    ///        server list.
    ///
    std::string getPlanetServerReplicationUrl() const
    {
        if (!planet_server.empty()) {
            return "https://" + planet_server + (datadir.empty() ? "" : "/" + datadir);
        } else {
            return planet_servers.front().replicationUrl();
        }
    }

    ///
    /// Returns a list of planet servers that support the given frequency.
    ///
    std::vector<PlanetServer> getPlanetServers(frequency_t frequency) const
    {
        std::vector<PlanetServer> servers;
        std::copy_if(planet_servers.begin(), planet_servers.end(), std::back_inserter(servers), [frequency](PlanetServer p) {
            return p.has_frequency(frequency);
        });
        return servers;
    }

    ///
    /// \brief dbConfigHelp
    /// \return a string with the names of the environment variables of the available configuration options and their current values.
    ///
    std::string dbConfigHelp() const
    {
        return str(format(R"raw(
REPLICATOR_OSMSTATS_DB_URL=%1%
REPLICATOR_UNDERPASS_DB_URL=%2%
REPLICATOR_TASKINGMANAGER_DB_URL=%3%
REPLICATOR_OSM2PGSQL_DB_URL=%3%
REPLICATOR_FREQUENCY=%4%
REPLICATOR_TASKINGMANAGER_USERS_UPDATE_FREQUENCY=%5%
      )raw") % osmstats_db_url %
                   underpass_db_url % taskingmanager_db_url % osm2pgsql_db_url %
                   Underpass::freq_to_string(frequency) % taskingmanager_users_update_frequency);
    };
};

} // namespace replicatorconfig

#endif // EOF __REPLICATOR_CONFIG_HH__
