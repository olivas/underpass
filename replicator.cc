//
// Copyright (c) 2020, Humanitarian OpenStreetMap Team
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "hotconfig.h"
#endif

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <pqxx/pqxx>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <cassert>
#include <deque>
#include <list>
#include <thread>

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/program_options.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/filesystem.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filtering_stream.hpp>

using namespace boost;
namespace opts = boost::program_options;

// #include "hotosm.hh"
#include "osmstats/changeset.hh"
// #include "osmstats/osmstats.hh"
#include "data/geoutil.hh"
// #include "osmstats/replication.hh"
#include "data/import.hh"
#include "data/threads.hh"

#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1

// Forward declarations
namespace changeset {
  class ChangeSet;
};

/// A helper function to simplify the main part.
template<class T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v)
{
    copy(v.begin(), v.end(), std::ostream_iterator<T>(os, " "));
    return os;
}

/// \class Replicator
/// \brief This class does all the actual work
///
/// This class identifies, downloads, and processes a replication file.
/// Replication files are available from the OSM planet server.
class Replicator : public replication::Replication, public Timer
{
public:
    /// Create a new instance, and read in the geoboundaries file.
    bool initializeData(void) {
        auto hashes = std::make_shared<std::map<std::string, int>>();
        auto geou = std::make_shared<geoutil::GeoUtil>();
        geou->startTimer();
        // FIXME: this shouldn't be hardcoded
        geou->readFile("../underpass.git/data/geoboundaries.osm", true);
        geou->endTimer("Replicator");
        changes = std::make_shared<changeset::ChangeSetFile>();
        changes->setupBoundaries(geou);

        // FIXME: should return a real value
        return false;
    };
    
    /// Initialize the raw_user, raw_hashtags, and raw_changeset tables
    /// in the OSM stats database from a changeset file
    bool initializeRaw(std::vector<std::string> &rawfile, const std::string &database) {
        for (auto it = std::begin(rawfile); it != std::end(rawfile); ++it) {
            changes->importChanges(*it);
        }
        // FIXME: return a real value
        return false;
    };

    /// Get the value for a hashtag
    int lookupHashID(const std::string &hash) {
        auto found = hashes->find(hash);
        if (found != hashes->end()) {
            return (*hashes)[hash];
        } else {
            int id = ostats.lookupHashtag(hash);
            return id;
        }

        return 0;
    };
    /// Get the numeric ID of the country by name
    long lookupCountryID(const std::string &country) {
        return geou->getCountry(country).getID();
    };

    // osmstats::RawCountry & findCountry() {
    //     geou.inCountry();
    // };

private:
    osmstats::QueryOSMStats ostats;                    ///< OSM Stats database access
    std::shared_ptr<changeset::ChangeSetFile> changes; ///< All the changes in the file
    std::shared_ptr<geoutil::GeoUtil> geou;             ///< Country boundaries
    std::shared_ptr<std::map<std::string, int>> hashes; ///< Existing hashtags
};

int
main(int argc, char *argv[])
{
    // Store the file names for replication files
    std::string changeset;
    std::string osmchange;
    // The update frequency between downloads
    std::string interval = "minutely";
    //bool encrypted = false;
    long sequence = 0;
    ptime timestamp(not_a_date_time);

    opts::positional_options_description p;
    opts::variables_map vm;
     try {
        opts::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "display help")
            ("server,s", "database server (defaults to localhost)")
            ("statistics,s", "OSM Stats database name (defaults to osmstats)")
            ("url,u", opts::value<std::string>(), "Starting URL")
            ("frequency,f", opts::value<std::string>(), "Update frequency (hour, daily), default minute)")
            ("timestamp,t", opts::value<std::string>(), "Starting timestamp")
            ("changeset,c", opts::value<std::vector<std::string>>(), "Initialize OSM Stats with changeset")
            ("osmchange,o", opts::value<std::vector<std::string>>(), "Apply osmchange to OSM Stats")
            ("import,i", opts::value<std::string>(), "Initialize pgsnapshot database with datafile")
//            ("osm,o", "OSM database name (defaults to pgsnapshot)")
//            ("url,u", opts::value<std::string>(), "Replication File URL")
//            ("encrypted,e", "enable HTTPS (the default)")
//            ("format,f", "database format (defaults to pgsnapshot)")
//            ("verbose,v", "enable verbosity")
            ;
        
        opts::store(opts::command_line_parser(argc, argv).
                  options(desc).positional(p).run(), vm);
        opts::notify(vm);

        if (vm.count("help")) {
            std::cout << "Usage: options_description [options]\n";
            std::cout << desc;
            return 0;
        }
     } catch(std::exception& e) {
         std::cout << e.what() << "\n";
         return 1;
     }

     Replicator replicator;
     replicator.initializeData();
     std::vector<std::string> rawfile;
     std::shared_ptr<std::vector<unsigned char>> data;

     // A changeset has the hashtags and comments we need. Multiple URLS
     // or filename may be specified on the command line, common when
     // catching up on changes.
     if (vm.count("changeset")) {
         std::vector<std::string> files = vm["changeset"].as<std::vector<std::string>>();
         if (files[0].substr(0, 4) == "http") {
             data = replicator.downloadFiles(files, true);
         } else {
             for (auto it = std::begin(files); it != std::end(files); ++it) {
                 replicator.readChanges(*it);
             }
         }
     }
     if (vm.count("osmchange")) {
         std::vector<std::string> files = vm["osmchange"].as<std::vector<std::string>>();
         if (files[0].substr(0, 4) == "http") {
             data = replicator.downloadFiles(files, false);
         } else {
             for (auto it = std::begin(files); it != std::end(files); ++it) {
                 replicator.readChanges(*it);
             }
         }
     }
     if (vm.count("sequence")) {
         std::cout << "Sequence is " << vm["sequence"].as<int>() << std::endl;
     }
     replication::Planet planet;
     if (vm.count("url")) {
         planet.connectDB("underpass");
         auto first = planet.getFirstState();
         std::string url = vm["url"].as<std::string>();
         auto links =  std::make_shared<std::vector<std::string>>();
         links = planet.scanDirectory(url);
         for (auto it = std::rbegin(*links); it != std::rend(*links); ++it) {
             // if (*it == "000/" || *it == "001/") {
             //     continue;
             // }
             if (it->empty()) {
                 continue;
             }
             if (it->at(0) >= 48 &&  it->at(0) <= 57) {
                 std::cout << "Directory: " << url + *it << std::endl;
             }
             if (*it != "state.yaml") {
                 auto slinks =  std::make_shared<std::vector<std::string>>();
                 slinks = planet.scanDirectory(url + *it);
                 // Looping backwards means we start with 000, and end with 999. Oldest data
                 // first.
                 for (auto sit = std::rbegin(*slinks); sit != std::rend(*slinks); ++sit) {
                     if (sit->empty()) {
                         continue;
                     }
                     planet.startTimer();
                     std::string subdir = url + *it + *sit;
                     std::cout << "Sub Directory: " << subdir << std::endl;
                     auto flinks = planet.scanDirectory(subdir);
                     threads::ThreadManager tmanager;
#ifdef USE_MULTI     // Multi-threaded
                     std::thread tstate (threads::startStateThreads, std::ref(subdir),
                                         std::ref(*flinks));
#else     // Single threaded
                     for (auto fit = std::rbegin(*flinks); fit != std::rend(*flinks); ++fit) {
                         std::string subpath = subdir + fit->substr(0, 3);
                         std::shared_ptr<replication::StateFile> exists = planet.getState(subpath);
                         // boost::filesystem::path path(first->path);
                         std::string suffix = boost::filesystem::extension(*fit);
                         if (!exists->path.empty()) {
                             std::cout << "Already have: " << subdir << *fit<< std::endl;
                             continue;
                         }
                         if (suffix == ".txt") {
                             data = replicator.downloadFiles(subdir + *fit, true);
                             if (!data) {
                                 std::cout << "File not found: " << subdir << *fit << std::endl;
                                 continue;
                                 // exit(-1);
                             } else {
                                 std::string tmp(reinterpret_cast<const char *>(data->data()));
                                 replication::StateFile state(tmp, true);
                                 state.path = subpath;
                                 state.dump();
                                 planet.changeset[state.timestamp] = subdir;
                                 planet.writeState(state);
                             }
                         }
                     }
#endif
                     tstate.join();
                     planet.endTimer("main");
                 }
             }
         }
         // links->clear();
     }

     //replicator.startTimer();
     planet.startTimer();
     if (vm.count("timestamp")) {
         std::cout << "Timestamp is: " << vm["timestamp"].as<std::string> () << "\n";
         timestamp = time_from_string(vm["timestamp"].as<std::string>());
         std::string foo = planet.findData(replication::minutely, timestamp);
         std::cout << "Full path: " << foo << std::endl;
         data = replicator.downloadFiles("https://planet.openstreetmap.org/replication/minute" + foo + "/000/000.state.txt", true);
         if (data->empty()) {
             std::cout << "File not found" << std::endl;
             exit(-1);
         }
         std::string tmp(reinterpret_cast<const char *>(data->data()));
         replication::StateFile state(tmp, true);
         state.dump();
     }

     planet.endTimer("stats");
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
     // FIXME: add logging
     if (vm.count("verbose")) {
         std::cout << "Verbosity enabled.  Level is " << vm["verbose"].as<int>() << std::endl;
     }

     if (sequence > 0 and !timestamp.is_not_a_date_time()) {
        std::cout << "ERROR: Can only specify a timestamp or a sequence" << std::endl;
        exit(1);
    }

    // replication::Replication rep(server, timestamp, sequence);
    replication::Replication rep("https://planet.openstreetmap.org", timestamp, sequence);


    
    // std::vector<std::string> files;
    // files.push_back("004/139/998.state.txt");
    // std::shared_ptr<std::vector<std::string>> links = rep.downloadFiles(files, true);
    // std::cout << "Got "<< links->size() << " directories" << std::endl;
    // // links = rep.downloadFiles(*links, true);
    // // std::cout << "Got "<< links->size() << " directories" << std::endl;

    // changeset::StateFile foo("/tmp/foo1.txt", false);
    // changeset::StateFile bar("/tmp/foo2.txt", false);
    // return 0;
}

