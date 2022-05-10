//
// Copyright (c) 2020, 2021, 2022 Humanitarian OpenStreetMap Team
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

#include <dejagnu.h>
#include "galaxy/osmchange.hh"
#include <boost/geometry.hpp>
#include <boost/program_options.hpp>
#include "data/geoutil.hh"
#include <iostream>
#include "log.hh"
#include "replicatorconfig.hh"
#include "galaxy/planetreplicator.hh"

namespace opts = boost::program_options;

using namespace replicatorconfig;
using namespace planetreplicator;
using namespace logger;

class TestOsmChanges : public osmchange::OsmChangeFile {
};

class TestCO : public osmchange::OsmChangeFile {
};

class TestPlanet : public replication::Planet {
};

void
collectStats(opts::variables_map vm) {

    ReplicatorConfig config;    
    if (vm.count("timestamp")) {
        auto timestamp = vm["timestamp"].as<std::string>();
        config.start_time = from_iso_extended_string(timestamp);
    } else {
        config.start_time = from_iso_extended_string("2022-01-01T00:00:00");
    }
    config.planet_server = config.planet_servers[0].domain + "/replication";
    std::string boundary = "priority.geojson";
    if (vm.count("boundary")) {
        boundary = vm["boundary"].as<std::string>();
    }
    geoutil::GeoUtil geou;
    multipolygon_t poly;
    if (!geou.readFile(boundary)) {
        log_debug(_("Could not find '%1%' area file!"), boundary);
        boost::geometry::read_wkt("MULTIPOLYGON(((-180 90,180 90, 180 -90, -180 -90,-180 90)))", poly);
    } else {
        poly = geou.boundary;
    }
 
    int increment;
    if (vm.count("increment")) {
        const auto str_increment = vm["increment"].as<std::string>();
        increment = std::stoi(str_increment) + 1;
    } else { 
        increment = 2;
    }
    planetreplicator::PlanetReplicator replicator;
    auto osmchange = replicator.findRemotePath(config, config.start_time);
    
    std::string jsonstr = "[";

    while (--increment) {
        TestCO change;
        if (boost::filesystem::exists(osmchange->filespec)) {
            change.readChanges(osmchange->filespec);
        } else { 
            TestPlanet planet;
            auto data = planet.downloadFile(osmchange->getURL());
            auto xml = planet.processData(osmchange->filespec, *data);
            std::istream& input(xml);
            change.readXML(input);
        }
        change.areaFilter(poly);
        auto stats = change.collectStats(poly);

        for (auto it = std::begin(*stats); it != std::end(*stats); ++it) {
            auto changestats = it->second;
            jsonstr += "{\"changeset_id\":" + std::to_string(changestats->change_id);

            if (changestats->added.size() > 0) {
                jsonstr += ", \"added\":[";
                for (const auto &added: std::as_const(changestats->added)) {
                    if (added.second > 0) {
                        jsonstr += "{\"" + added.first + "\":" + std::to_string(added.second) + "},";
                    }
                }
                jsonstr.erase(jsonstr.size() - 1);
                jsonstr += "]";
            } else {
                jsonstr += ", \"added\": []";
            }

            if (changestats->modified.size() > 0) {
                jsonstr += ", \"modified\":[";
                for (const auto &modified: std::as_const(changestats->modified)) {
                    if (modified.second > 0) {
                        jsonstr += "{\"" + modified.first + "\":" + std::to_string(modified.second) + "},";
                    }
                }
                jsonstr.erase(jsonstr.size() - 1);
                jsonstr += "]},\n";
            } else {
                jsonstr += ", \"modified\": []},\n";
            }

        }

        osmchange->Increment();
    }

    jsonstr.erase(jsonstr.size() - 2);
    jsonstr += "\n]";
    std::cout << jsonstr << std::endl;

}

std::shared_ptr<std::map<long, std::shared_ptr<osmchange::ChangeStats>>>
getStatsFromFile(std::string filename) {
    TestOsmChanges osmchanges;
    osmchanges.readChanges(filename);
    multipolygon_t poly;
    boost::geometry::read_wkt("MULTIPOLYGON(((-180 90,180 90, 180 -90, -180 -90,-180 90)))", poly);
    auto stats = osmchanges.collectStats(poly);
    return stats;    
}

std::map<std::string, long>
getValidationStatsFromFile(std::string filename) {
    yaml::Yaml yaml;
    yaml.read(filename);
    std::map<std::string, long> config;
    for (auto it = std::begin(yaml.config); it != std::end(yaml.config); ++it) {
        auto keyvalue = std::pair<std::string,long>(it->first, std::stol(yaml.getConfig(it->first)));
        config.insert(keyvalue);
    }
    return config;
}
void
testStat(std::shared_ptr<osmchange::ChangeStats> changestats, std::map<std::string, long> validation, std::string tag) {

    TestState runtest;

    logger::LogFile &dbglogfile = logger::LogFile::getDefaultInstance();
    dbglogfile.setWriteDisk(true);
    dbglogfile.setLogFilename("stats-test.log");
    dbglogfile.setVerbosity(3);

    if (changestats->added.size() > 0) {
        if (changestats->added.count(tag)) {
            std::cout << "added_ " + tag + "(stats): " << changestats->added.at(tag) << std::endl;
            std::cout << "added_" + tag + " (validation): " << validation.at("added_" + tag) << std::endl;
            if (changestats->added.at(tag) == validation.at("added_" + tag)) {
                runtest.pass("Calculating added (created) " + tag);
            } else{
                runtest.fail("Calculating added (created) " + tag);
            }
        }
    }
    if (changestats->modified.size() > 0) {
        if (changestats->modified.count(tag)) {
            std::cout << "modified_" + tag + " (stats): " << changestats->modified.at(tag) << std::endl;
            std::cout << "modified_" + tag + " (validation): " << validation.at("modified_" + tag) << std::endl;
            if (changestats->modified.at(tag) == validation.at("modified_" + tag)) {
                runtest.pass("Calculating modified " + tag);
            } else{
                runtest.fail("Calculating modified " + tag);
            }
        }
    }
}

void
validateStatsFromFile(std::vector<std::string> files) {
    std::string statsFile(DATADIR);
    statsFile += "/testsuite/testdata/" + files.at(0);
    auto stats = getStatsFromFile(statsFile);

    std::string validationFile(DATADIR);
    validationFile += "/testsuite/testdata/" + files.at(1);
    auto validation = getValidationStatsFromFile(validationFile);

    for (auto it = std::begin(*stats); it != std::end(*stats); ++it) {
        auto changestats = it->second;
        if (changestats->change_id == validation.at("change_id")) {
            std::cout << "change_id: " << changestats->change_id << std::endl;
            testStat(changestats, validation, "highway");
            testStat(changestats, validation, "building");
            testStat(changestats, validation, "waterway");
        }
    }
}

void runTests() {
    std::vector<std::string> files = {"test_stats.osc", "test_stats.yaml"};
    validateStatsFromFile(files);
}

int
main(int argc, char *argv[]) {
    // Declare the supported options.
    opts::positional_options_description p;
    opts::variables_map vm;
    opts::options_description desc("Allowed options");
    desc.add_options()
        ("mode,m", opts::value<std::string>(), "Mode (collect-stats)")
        ("file,f", opts::value<std::vector<std::string>>(), "OsmChange file, YAML file with expected values")
        ("timestamp,t", opts::value<std::string>(), "Starting timestamp (default: 2022-01-01T00:00:00)")
        ("increment,i", opts::value<std::string>(), "Number of increments to do (default: 1)")
        ("boundary,b", opts::value<std::string>(), "Boundary polygon file name");

    opts::store(opts::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
    opts::notify(vm);

    if (vm.count("mode")) {
        if (vm["mode"].as<std::string>() == "collect-stats") {
            collectStats(vm);
        }
    } else if (vm.count("file")) {
        auto files = vm["file"].as<std::vector<std::string>>();
        validateStatsFromFile(files);
    } else {
        runTests();
    }

}

// local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
