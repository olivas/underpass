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

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <pqxx/pqxx>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "replicator/planetreplicator.hh"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/filesystem.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/regex.hpp>
#include <boost/timer/timer.hpp>

using namespace boost;
namespace opts = boost::program_options;

#include "utils/geoutil.hh"
#include "utils/log.hh"
#include "osm/changeset.hh"
#include "osm/osmchange.hh"
#include "replicator/threads.hh"
#include "bootstrap/bootstrap.hh"
#include "underpassconfig.hh"

using namespace querystats;
using namespace underpassconfig;

#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1

// Exit code when connection to DB fails
#define EXIT_DB_FAILURE -1

using namespace logger;

// Forward declarations
namespace changesets {
    class ChangeSet;
};

int
main(int argc, char *argv[])
{

    // The changesets URL path (e.g. "/001/001/999")
    std::string starting_url_path;

    std::string datadir = "replication/";
    std::string boundary = PKGLIBDIR;
    boundary += "/config/priority.geojson";

    UnderpassConfig config;

    opts::positional_options_description p;
    opts::variables_map vm;
    opts::options_description desc("Allowed options");

    try {
        // clang-format off
        desc.add_options()
            ("help,h", "display help")
            ("server,s", opts::value<std::string>(), "Database server for replicator output (defaults to localhost/underpass) "
                                                     "can be a hostname or a full connection string USER:PASSSWORD@HOST/DATABASENAME")
            ("planet,p", opts::value<std::string>(), "Replication server (defaults to planet.maps.mail.ru)")
            ("url,u", opts::value<std::string>(), "Starting URL path (ex. 000/075/000), takes precedence over 'timestamp' option")
            ("changeseturl", opts::value<std::string>(), "Starting URL path for ChangeSet (ex. 000/075/000), takes precedence over 'timestamp' option")
            ("frequency,f", opts::value<std::string>(), "Update frequency (hourly, daily), default minutely)")
            ("timestamp,t", opts::value<std::vector<std::string>>(), "Starting timestamp (can be used 2 times to set a range)")
            ("import,i", opts::value<std::string>(), "Initialize OSM database with datafile")
            ("boundary,b", opts::value<std::string>(), "Boundary polygon file name")
            ("osmnoboundary", "Disable boundary polygon for OsmChanges")
            ("oscnoboundary", "Disable boundary polygon for Changesets")
            ("datadir", opts::value<std::string>(), "Base directory for cached files (with ending slash)")
            ("verbose,v", "Enable verbosity")
            ("logstdout,l", "Enable logging to stdout, default is log to underpass.log")
            ("changefile", opts::value<std::string>(), "Import change file")
            ("concurrency,c", opts::value<std::string>(), "Concurrency")
            ("changesets", "Changesets only")
            ("osmchanges", "OsmChanges only")
            ("debug,d", "Enable debug messages for developers")
            ("disable-stats", "Disable statistics")
            ("disable-validation", "Disable validation")
            ("disable-raw", "Disable raw OSM data")
            ("norefs", "Disable refs (useful for non OSM data)")
            ("bootstrap", "Bootstrap data tables");
        // clang-format on

        opts::store(opts::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        opts::notify(vm);

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    if (vm.count("norefs")) {
        config.norefs = true;
    }

    // Logging
    logger::LogFile &dbglogfile = logger::LogFile::getDefaultInstance();
    if (vm.count("verbose")) {
        dbglogfile.setVerbosity();
    }
    if (!vm.count("logstdout")) {
        dbglogfile.setWriteDisk(true);
        dbglogfile.setLogFilename("underpass.log");
    }
    if (vm.count("debug")) {
        dbglogfile.setVerbosity();
    }

    if (vm.count("server")) {
        config.underpass_db_url = vm["server"].as<std::string>();
    }

    // Concurrency
    if (vm.count("concurrency")) {
        const auto concurrency = vm["concurrency"].as<std::string>();
        try {
            config.concurrency = std::stoi(concurrency);
            log_debug("Hardware threads: %1%", std::thread::hardware_concurrency());
            if (config.concurrency > std::thread::hardware_concurrency()) {
                log_error("ERROR: concurrency cannot exceed the number of threads supported by hardware (%1%)!", std::thread::hardware_concurrency());
            }
        } catch (const std::exception &) {
            log_error("ERROR: error parsing \"concurrency\"!");
            exit(-1);
        }
    } else {
        config.concurrency = std::thread::hardware_concurrency();
    }
    
    if (vm.count("timestamp") || vm.count("url")) {
        // Planet server
        if (vm.count("planet")) {
            config.planet_server = vm["planet"].as<std::string>();
            // Strip https://
            if (config.planet_server.find("https://") == 0) {
                config.planet_server = config.planet_server.substr(8);
            }

            if (boost::algorithm::ends_with(config.planet_server, "/")) {
                config.planet_server.resize(config.planet_server.size() - 1);
            }
        } else {
            config.planet_server = config.planet_servers[0].domain;
        }

        if (vm.count("boundary")) {
            boundary = vm["boundary"].as<std::string>();
        }

        // Boundary
        geoutil::GeoUtil geou;
        if (!geou.readFile(boundary)) {
            log_debug("Could not find '%1%' area file!", boundary);
        }

        // Features
        if (vm.count("disable-validation")) {
            config.disable_validation = true;
        }
        if (vm.count("disable-stats")) {
            config.disable_stats = true;
        }
        if (vm.count("disable-raw")) {
            config.disable_raw = true;
        }

        // Replication
        planetreplicator::PlanetReplicator replicator;

        std::shared_ptr<std::vector<unsigned char>> data;

        if (!starting_url_path.empty() && vm.count("timestamp")) {
            log_debug("ERROR: 'url' takes precedence over 'timestamp' arguments are mutually exclusive!");
            exit(-1);
        }

        // This is the default data directory on that server
        if (vm.count("datadir")) {
            datadir = vm["datadir"].as<std::string>();
        }
        const char *tmp = std::getenv("DATADIR");
        if (tmp != 0) {
            datadir = tmp;
        }

        // Add datadir to config
        config.datadir = datadir;

        if (vm.count("frequency")) {
            const auto strfreq = vm["frequency"].as<std::string>();
            if (strfreq[0] == 'm') {
                config.frequency = replication::minutely;
            } else if (strfreq[0] == 'h') {
                config.frequency = replication::hourly;
            } else if (strfreq[0] == 'd') {
                config.frequency = replication::daily;
            } else {
                log_debug("Invalid frequency!");
                exit(-1);
            }
        }

        auto osmchange = std::make_shared<RemoteURL>();
        // Specify a timestamp used by other options
        if (vm.count("timestamp")) {
            try {
                auto timestamps = vm["timestamp"].as<std::vector<std::string>>();
                if (timestamps[0] == "now") {
                    config.start_time = boost::posix_time::second_clock::universal_time();
                } else {
                    config.start_time = from_iso_extended_string(timestamps[0]);
                    if (timestamps.size() > 1) {
                        config.end_time = from_iso_extended_string(timestamps[1]);
                    }
                }
                osmchange = replicator.findRemotePath(config, config.start_time);
            } catch (const std::exception &ex) {
                log_error("could not parse timestamps!");
                exit(-1);
            }
        } else if (vm.count("url")) {
            replicator.connectServer("https://" + config.planet_server);
            // This is the changesets path part (ex. 000/075/000), takes precedence over 'timestamp'
            // option. This only applies to the osm change files, as it's timestamp is used to
            // start the changesets.
            std::string fullurl = "https://" + config.planet_server + "/replication/" + StateFile::freq_to_string(config.frequency);
            std::vector<std::string> parts;
            boost::split(parts, vm["url"].as<std::string>(), boost::is_any_of("/"));
            // fullurl += "/" + vm["url"].as<std::string>() + "/" + parts[2] + ".state.txt";
            fullurl += "/" + vm["url"].as<std::string>() + ".state.txt";
            osmchange->parse(fullurl);
            auto data = replicator.downloadFile(*osmchange).data;
            StateFile start(osmchange->filespec, false);
            //start.dump();
            config.start_time = start.timestamp;
            boost::algorithm::replace_all(osmchange->filespec, ".state.txt", ".osc.gz");
        }

        std::thread changesetThread;
        std::thread osmChangeThread;

        multipolygon_t poly;
        if (!vm.count("changesets")) {
            multipolygon_t * osmboundary = &poly;
            if (!vm.count("osmnoboundary")) {
                osmboundary = &geou.boundary;
            }
            osmChangeThread = std::thread(replicatorthreads::startMonitorChanges, std::ref(osmchange),
                            std::ref(*osmboundary), std::ref(config));
        }
        config.frequency = replication::changeset;
        auto changeset = replicator.findRemotePath(config, config.start_time);
        if (vm.count("changeseturl")) {
            std::vector<std::string> parts;
            boost::split(parts, vm["changeseturl"].as<std::string>(), boost::is_any_of("/"));
            changeset->updatePath(stoi(parts[0]),stoi(parts[1]),stoi(parts[2]));
        }
        if (!vm.count("osmchanges")) {
            multipolygon_t * oscboundary = &poly;
            if (!vm.count("oscnoboundary")) {
                oscboundary = &geou.boundary;
            }
            changesetThread = std::thread(replicatorthreads::startMonitorChangesets, std::ref(changeset),
                            std::ref(*oscboundary), std::ref(config));
        }
        if (changesetThread.joinable()) {
            changesetThread.join();
        }
        if (osmChangeThread.joinable()) {
            osmChangeThread.join();
        }

        exit(0);

    }
    
    if (vm.count("bootstrap")){
        std::thread bootstrapThread;
        std::cout << "Starting bootstrapping proccess ..." << std::endl;
        bootstrapThread = std::thread(bootstrap::startProcessingWays, std::ref(config));
        log_info("Waiting...");
        if (bootstrapThread.joinable()) {
            bootstrapThread.join();
        }
        exit(0);
    }

    std::cout << "Usage: options_description [options]" << std::endl;
    std::cout << desc << std::endl;
    std::cout << "A few configuration options can be set through the "
                    "environment,"
                << std::endl;
    std::cout << "this is the current status of the configuration "
                    "options with"
                << std::endl;
    std::cout << "the environment variable names and their current "
                    "values (possibly defaults)."
                << std::endl;
    // std::cout << config.dbConfigHelp() << std::endl;
    exit(0);

}

// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
