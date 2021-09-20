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

using namespace boost;
namespace opts = boost::program_options;

// #include "hotosm.hh"
#include "data/geoutil.hh"
#include "osmstats/changeset.hh"
// #include "osmstats/replication.hh"
#include "data/import.hh"
#include "data/threads.hh"
#include "data/underpass.hh"
#include "log.hh"
#include "replicatorconfig.hh"
#include "timer.hh"

using namespace osmstats;
using namespace underpass;
using namespace replicatorconfig;

#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1

// Exit code when connection to DB fails
#define EXIT_DB_FAILURE -1

enum pathMatches { ROOT, DIRECTORY, SUBDIRECTORY, FILEPATH };

using namespace logger;

// Forward declarations
namespace changeset {
class ChangeSet;
};

/// A helper function to simplify the main part.
template <class T> std::ostream &
operator<<(std::ostream &os, const std::vector<T> &v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

/// \class Replicator
/// \brief This class does all the actual work
///
/// This class identifies, downloads, and processes a replication file.
/// Replication files are available from the OSM planet server.
class Replicator : public replication::Replication {
  public:
    /// Create a new instance, and read in the geoboundaries file.
    Replicator(void)
    {
        auto hashes = std::make_shared<std::map<std::string, int>>();
    };

    /// Initialize the raw_user, raw_hashtags, and raw_changeset tables
    /// in the OSM stats database from a changeset file
    bool initializeRaw(std::vector<std::string> &rawfile,
                       const std::string &database)
    {
        for (auto it = std::begin(rawfile); it != std::end(rawfile); ++it) {
            changes->importChanges(*it);
        }
        // FIXME: return a real value
        return false;
    };

#if 0
    std::string getLastPath(replication::frequency_t interval) {
        ptime last = ostats.getLastUpdate();
        Underpass under;
        auto state = under.getState(interval, last);
        return state->path;
    };
#endif
    // osmstats::RawCountry & findCountry() {
    //     geou.inCountry();

    enum pathMatches matchUrl(const std::string &url)
    {
        boost::regex test{"([0-9]{3})"};

        boost::sregex_token_iterator iter(url.begin(), url.end(), test, 0);
        boost::sregex_token_iterator end;

        std::vector<std::string> dirs;

        for (; iter != end; ++iter) {
            dirs.push_back(*iter);
        }

        pathMatches match;
        switch (dirs.size()) {
            default:
                match = pathMatches::ROOT;
                break;
            case 1:
                match = pathMatches::DIRECTORY;
                break;
            case 2:
                match = pathMatches::SUBDIRECTORY;
                break;
            case 3:
                match = pathMatches::FILEPATH;
                break;
        }

        return match;
    }

    //    std::string baseurl;                                ///< base URL for
    //    the planet server
  private:
    std::shared_ptr<changeset::ChangeSetFile>
        changes; ///< All the changes in the file
    std::shared_ptr<std::map<std::string, int>> hashes; ///< Existing hashtags
};

int
main(int argc, char *argv[])
{

    // Store the file names for replication files
    std::string changeset;
    std::string osmchange;
    // The update frequency between downloads
    // bool encrypted = false;
    long sequence = 0;
    ptime starttime(not_a_date_time);
    ptime endtime(not_a_date_time);
    std::string url;

    long tmusersfrequency{-1};
    std::string datadir = "replication/";
    std::string boundary = "priority.geojson";

    ReplicatorConfig replicator_config;

    opts::positional_options_description p;
    opts::variables_map vm;
    try {
        opts::options_description desc("Allowed options");
        // clang-format off
        desc.add_options()
            ("help,h", "display help")
            ("server,s", opts::value<std::string>(), "Database server for replicator output (defaults to localhost/osmstats) "
                                                     "can be a hostname or a full connection string USER:PASSSWORD@HOST/DATABASENAME")
            ("tmserver", opts::value<std::string>(), "Tasking Manager database server for input  (defaults to localhost/taskingmanager), "
                                                     "can be a hostname or a full connection string USER:PASSSWORD@HOST/DATABASENAME")
            ("upserver", opts::value<std::string>(), "Underpass database server for internal use (defaults to localhost/underpass), "
                                                     "can be a hostname or a full connection string USER:PASSSWORD@HOST/DATABASENAME")
            ("osm2pgsqlserver", opts::value<std::string>(), "Osm2pgsql database server (defaults to localhost), "
                                                     "can be a hostname or a full connection string USER:PASSSWORD@HOST/DATABASENAME")
            ("tmusersfrequency", opts::value<std::string>(), "Frequency in seconds for the Tasking Manager database users "
                                                             "synchronization: -1 (disabled), 0 (single shot), > 0 (interval in seconds)")
            ("planet,p", opts::value<std::string>(), "Replication server (defaults to planet.maps.mail.ru)")
            ("url,u", opts::value<std::string>(), "Starting URL path (ex. 000/075/000), takes precedence over 'timestamp' option")
            ("monitor,m", "Start monitoring")
            ("frequency,f", opts::value<std::string>(), "Update frequency (hourly, daily), default minutely)")
            ("timestamp,t", opts::value<std::vector<std::string>>(), "Starting timestamp")
            ("import,i", opts::value<std::string>(), "Initialize OSM database with datafile")
            ("boundary,b", opts::value<std::string>(), "Boundary polygon file name")
            ("datadir,i", opts::value<std::string>(), "Base directory for cached files")
            ("verbose,v", "Enable verbosity")
            ("logstdout,l", "Enable logging to stdout, default is log to underpass.log")
            ("changefile,c", opts::value<std::string>(), "Import change file")
            ("debug,d", "Enable debug messages for developers");
        // clang-format on

        opts::store(opts::command_line_parser(argc, argv)
                        .options(desc)
                        .positional(p)
                        .run(),
                    vm);
        opts::notify(vm);

        if (vm.count("help")) {
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
            std::cout << replicator_config.dbConfigHelp() << std::endl;
            return 0;
        }
    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

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

    // Osm2pgsql options
    if (vm.count("osm2pgsqlserver")) {
        replicator_config.osm2pgsql_db_url =
            vm["osm2pgsqlserver"].as<std::string>();
    }

    // Underpass DB for internal use
    if (vm.count("upserver")) {
        replicator_config.underpass_db_url = vm["upserver"].as<std::string>();
    }

    // Planet server
    if (vm.count("planet")) {
        replicator_config.planet_server = vm["planet"].as<std::string>();
        // Little cleanup: we want something like https://planet.maps.mail.ru
        if (replicator_config.planet_server.find("https://") != 0) {
            log_error("ERROR: planet server must start with 'https://' !");
            exit(-1);
        }

        if (boost::algorithm::ends_with(replicator_config.planet_server, "/")) {
            replicator_config.planet_server.resize(
                replicator_config.planet_server.size() - 1);
        }
    }

    // TM users options
    if (vm.count("tmserver")) {
        replicator_config.taskingmanager_db_url =
            vm["tmserver"].as<std::string>();
    }

    if (vm.count("tmusersfrequency")) {
        const auto freqValue{vm["tmusersfrequency"].as<std::string>()};
        char *p;
        const long converted = strtol(freqValue.c_str(), &p, 10);
        if (*p) {
            log_error("ERROR: You need to supply a valid integer for "
                      "tmusersfrequency!");
            exit(-1);
        } else {
            replicator_config.taskingmanager_users_update_frequency = converted;
        }
    }

    if (vm.count("boundary")) {
        boundary = vm["boundary"].as<std::string>();
    }

    if (vm.count("server")) {
        replicator_config.osmstats_db_url = vm["server"].as<std::string>();
    }

    geoutil::GeoUtil geou;
    std::string priority_area_file_path = SRCDIR;
    priority_area_file_path += "/data/" + boundary;
    if (!boost::filesystem::exists(priority_area_file_path)) {
        priority_area_file_path = PKGLIBDIR;
        priority_area_file_path += "/" + boundary;
    }

    if (!geou.readFile(priority_area_file_path)) {
        log_debug("Could not find 'priority.geojson' area file!");
    }

    // Tasking Manager users sync setup

    // Thread safe flag to exit the periodic sync loop
    std::atomic<bool> tmUserSyncIsActive{true};

    // RIIA Custom deleter because of multiple exit points
    auto stopTmUserSyncMonitor = [&tmUserSyncIsActive](std::thread *ptr) {
        if (ptr->joinable() && tmUserSyncIsActive) {
            tmUserSyncIsActive = false;
            ptr->join();
        }
        delete ptr;
    };

    std::unique_ptr<std::thread, decltype(stopTmUserSyncMonitor)>
        tmUserSyncMonitorThread(nullptr, stopTmUserSyncMonitor);
    if (replicator_config.taskingmanager_users_update_frequency >= 0) {
        tmUserSyncMonitorThread.reset(
            new std::thread(threads::threadTMUsersSync,
                            std::ref(tmUserSyncIsActive), replicator_config));
    }

    // End of Tasking Manager user sync setup

    Replicator replicator;
    if (vm.count("changefile")) {
        std::string file = vm["changefile"].as<std::string>();
        std::cout << "Importing change file " << file << std::endl;
        auto changeset = std::make_shared<changeset::ChangeSetFile>();
        changeset->readChanges(file);
        changeset->areaFilter(geou.boundary);
        osmstats::QueryOSMStats ostats;
        if (ostats.connect(replicator_config.osmstats_db_url)) {
            for (auto it = std::begin(changeset->changes);
                 it != std::end(changeset->changes); ++it) {
                ostats.applyChange(*it->get());
            }
        } else {
            log_error("ERROR: could not connect to osmstats DB, check 'server' "
                      "parameter!");
            exit(-1);
        }

        exit(0);
    }

    // replicator.initializeData();
    std::vector<std::string> rawfile;
    std::shared_ptr<std::vector<unsigned char>> data;

    // This is a full URL to the DB server with osmstats files
    if (vm.count("url")) {
        replicator_config.starting_url_path = vm["url"].as<std::string>();
    }

    // Make sure the path starts with a slash
    if (!replicator_config.starting_url_path.empty() &&
        replicator_config.starting_url_path[0] != '/') {
        replicator_config.starting_url_path.insert(0, 1, '/');
    }

    // This is the default data directory on that server
    if (vm.count("datadir")) {
        datadir = vm["datadir"].as<std::string>();
    }
    const char *tmp = std::getenv("DATADIR");
    if (tmp != 0) {
        datadir = tmp;
    }

    if (vm.count("frequency")) {
        const auto strfreq = vm["frequency"].as<std::string>();
        if (strfreq[0] == 'm') {
            replicator_config.frequency = replication::minutely;
        } else if (strfreq[0] == 'h') {
            replicator_config.frequency = replication::hourly;
        } else if (strfreq[0] == 'd') {
            replicator_config.frequency = replication::daily;
        } else {
            std::cerr << "ERROR: Invalid frequency!" << std::endl;
            exit(-1);
        }
    }

    const std::string fullurl{
        replicator_config.planet_server + "/" + datadir +
        Underpass::freq_to_string(replicator_config.frequency) +
        replicator_config.starting_url_path};
    replication::RemoteURL remote(fullurl);
    //     remote.dump();
    // Specify a timestamp used by other options
    if (vm.count("timestamp")) {
        auto timestamps = vm["timestamp"].as<std::vector<std::string>>();
        if (timestamps[0] == "now") {
            starttime = boost::posix_time::microsec_clock::local_time();
        } else {
            starttime = time_from_string(timestamps[0]);
            if (timestamps.size() > 1) {
                endtime = time_from_string(timestamps[1]);
            }
        }
    }

    // Check connection to underpass DB
    Underpass under;
    if (!under.connect(replicator_config.underpass_db_url)) {
        log_error("ERROR: could not connect to underpass DB, check 'upserver' "
                  "parameter!");
    }

    replication::Planet planet(remote);
    std::string clast;

    if (vm.count("monitor")) {
        if (starttime.is_not_a_date_time() &&
            replicator_config.starting_url_path.size() == 0) {
            log_error("ERROR: You need to supply either a timestamp or URL!");
            //         if (timestamp.is_not_a_date_time()) {
            //             timestamp = ostats.getLastUpdate();
            //         }

            exit(-1);
        }

        std::thread osmchanges_updates_thread;
        std::thread changesets_thread;

        if (!replicator_config.starting_url_path.empty()) {

            // remote.dump();
            osmchanges_updates_thread = std::thread(
                threads::startMonitor, std::ref(remote),
                std::ref(geou.boundary), std::ref(replicator_config));

            auto state = planet.fetchData(replicator_config.frequency,
                                          replicator_config.starting_url_path,
                                          replicator_config.underpass_db_url);

            if (!state->isValid()) {
                std::cerr << "ERROR: Invalid state from path!"
                          << replicator_config.starting_url_path << std::endl;
                exit(-1);
            }

            auto state2 =
                planet.fetchData(replication::changeset, state->timestamp,
                                 replicator_config.underpass_db_url);
            if (!state2->isValid()) {
                std::cerr << "ERROR: No changeset path!" << std::endl;
                exit(-1);
            }

            state2->dump();
            clast = replicator_config.planet_server + "/" + datadir +
                    "changesets" + state2->path;
            remote.parse(clast);
            // remote.dump();
            changesets_thread = std::thread(
                threads::startMonitor, std::ref(remote),
                std::ref(geou.boundary), std::ref(replicator_config));
        } else if (!starttime.is_not_a_date_time()) {
            // No URL, use the timestamp
            auto state = under.getState(replicator_config.frequency, starttime);
            if (state->isValid()) {
                auto tmp =
                    planet.fetchData(replicator_config.frequency, starttime,
                                     replicator_config.underpass_db_url);
                if (tmp->path.empty()) {
                    std::cerr << "ERROR: No last path!" << std::endl;
                    exit(-1);
                } else {
                    // last = replicator.baseurl +
                    // under.freq_to_string(frequency) + "/" + tmp->path;
                }
            } else {
                // last = replicator.baseurl + under.freq_to_string(frequency) +
                // "/" + state->path;
            }
            log_debug(_("Last minutely is "),
                      replicator_config.starting_url_path);
            // mthread = std::thread(threads::startMonitor, std::ref(remote));
        }

        log_info(_("Waiting..."));
        if (changesets_thread.joinable()) {
            changesets_thread.join();
        }
        if (osmchanges_updates_thread.joinable()) {
            osmchanges_updates_thread.join();
        }
        exit(0);
    }

    std::string statistics;
    if (vm.count("initialize")) {
        rawfile = vm["initialize"].as<std::vector<std::string>>();
        replicator.initializeRaw(rawfile, statistics);
    }
    std::string osmdb;
    if (vm.count("osm")) {
        osmdb = vm["osm"].as<std::vector<std::string>>()[0];
    }
    if (vm.count("import")) {
        std::string file = vm["import"].as<std::string>();
        import::ImportOSM osm(file, osmdb);
    }
    if (sequence > 0 and !starttime.is_not_a_date_time()) {
        log_error(_("Can only specify a timestamp or a sequence"));
        exit(1);
    }

    // replication::Replication rep(starttime, sequence);

    // std::vector<std::string> files;
    // files.push_back("004/139/998.state.txt");
    // std::shared_ptr<std::vector<std::string>> links =
    // rep.downloadFiles(files, true); std::cout << "Got "<< links->size() << "
    // directories" << std::endl;
    // // links = rep.downloadFiles(*links, true);
    // // std::cout << "Got "<< links->size() << " directories" << std::endl;

    // changeset::StateFile foo("/tmp/foo1.txt", false);
    // changeset::StateFile bar("/tmp/foo2.txt", false);
    // return 0;

    if (tmUserSyncMonitorThread) {
        if (tmUserSyncMonitorThread->joinable() && tmUserSyncIsActive) {
            tmUserSyncMonitorThread->join();
        }
    }
}

// Local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
