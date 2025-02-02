//
// Copyright (c) 2024 Humanitarian OpenStreetMap Team
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
#include <iostream>
#include "utils/log.hh"
#include "osm/osmobjects.hh"
#include "raw/queryraw.hh"
#include <string.h>
#include "replicator/replication.hh"
#include <boost/geometry.hpp>

using namespace replication;
using namespace logger;
using namespace queryraw;

TestState runtest;
class TestOsmChange : public osmchange::OsmChangeFile {};

class TestPlanet : public Planet {
  public:
    TestPlanet() = default;

    //! Clear the test DB and fill it with with initial test data
    bool init_test_case(const std::string &dbconn)
    {
        source_tree_root = getenv("UNDERPASS_SOURCE_TREE_ROOT")
                               ? getenv("UNDERPASS_SOURCE_TREE_ROOT")
                               : "../";

        const std::string test_replication_db_name{"underpass_test"};
        {
            pqxx::connection conn{dbconn + " dbname=template1"};
            pqxx::nontransaction worker{conn};
            worker.exec0("DROP DATABASE IF EXISTS " + test_replication_db_name);
            worker.exec0("CREATE DATABASE " + test_replication_db_name);
            worker.commit();
        }

        {
            pqxx::connection conn{dbconn + " dbname=" + test_replication_db_name};
            pqxx::nontransaction worker{conn};
            worker.exec0("CREATE EXTENSION postgis");
            worker.exec0("CREATE EXTENSION hstore");

            // Create schema
            const auto schema_path{source_tree_root + "setup/db/underpass.sql"};
            std::ifstream schema_definition(schema_path);
            std::string sql((std::istreambuf_iterator<char>(schema_definition)),
                            std::istreambuf_iterator<char>());

            assert(!sql.empty());
            worker.exec0(sql);
        }

        return true;
    };

    std::string source_tree_root;
};

bool processFile(const std::string &filename, std::shared_ptr<Pq> &db) {
    auto queryraw = std::make_shared<QueryRaw>(db);
    auto osmchanges = std::make_shared<osmchange::OsmChangeFile>();
    std::string destdir_base = DATADIR;
    multipolygon_t poly;
    osmchanges->readChanges(destdir_base + "/testsuite/testdata/raw/" + filename);
    queryraw->buildGeometries(osmchanges, poly);
    std::string rawquery;

    for (auto it = std::begin(osmchanges->changes); it != std::end(osmchanges->changes); ++it) {
        osmchange::OsmChange *change = it->get();
        // Nodes
        for (auto nit = std::begin(change->nodes); nit != std::end(change->nodes); ++nit) {
            osmobjects::OsmNode *node = nit->get();
            rawquery += queryraw->applyChange(*node);
        }
        // Ways
        for (auto wit = std::begin(change->ways); wit != std::end(change->ways); ++wit) {
            osmobjects::OsmWay *way = wit->get();
            rawquery += queryraw->applyChange(*way);
        }
        // Relations
        for (auto rit = std::begin(change->relations); rit != std::end(change->relations); ++rit) {
            osmobjects::OsmRelation *relation = rit->get();
            rawquery += queryraw->applyChange(*relation);
        }
    }
    db->query(rawquery);
}

const std::map<long, std::string> expectedGeometries = {
    {101874, "POLYGON((21.726001473 4.62042952837,21.726086573 4.62042742837,21.726084973 4.62036492836,21.725999873 4.62036702836,21.726001473 4.62042952837))"},
    {101875, "POLYGON((21.726001473 4.62042952837,21.726086573 4.62042742837,21.726084973 4.62036492836,21.725999873 4.62036702836,21.726001473 4.62042952837))"},
    {101875-2, "POLYGON((21.72600148 4.62042953,21.726086573 4.62042742837,21.726084973 4.62036492836,21.725999873 4.62036702836,21.72600148 4.62042953))"},
    {211766, "MULTIPOLYGON(((21.72600148 4.62042953,21.726086573 4.62042742837,21.726084973 4.62036492836,21.725999873 4.62036702836,21.72600148 4.62042953),(21.7260170728 4.62041343508,21.7260713875 4.62041326798,21.7260708846 4.62037684165,21.7260165699 4.62038035061,21.7260170728 4.62041343508)))"},
    {211766-2, "MULTIPOLYGON(((21.72600148 4.62042953,21.726086573 4.62042742837,21.7260807753 4.62037032501,21.725999873 4.62036702836,21.72600148 4.62042953),(21.7260170728 4.62041343508,21.7260713875 4.62041326798,21.7260708846 4.62037684165,21.7260165699 4.62038035061,21.7260170728 4.62041343508)))"},
    {211776, "MULTILINESTRING((21.726001473 4.62042952837,21.726086573 4.62042742837,21.726084973 4.62036492836,21.725999873 4.62036702836,21.726001473 4.62042952837))"}
};

std::string
getWKT(const polygon_t &polygon) {
    std::stringstream ss;
    ss << std::setprecision(12) << boost::geometry::wkt(polygon);
    return ss.str();
}

std::string
getWKTFromDB(const std::string &table, const long id, std::shared_ptr<Pq> &db) {
    auto result = db->query("SELECT ST_AsText(geom, 4326) from relations where osm_id=" + std::to_string(id));
    for (auto r_it = result.begin(); r_it != result.end(); ++r_it) {
        return (*r_it)[0].as<std::string>();
    }
}

int
main(int argc, char *argv[])
{
    logger::LogFile &dbglogfile = logger::LogFile::getDefaultInstance();
    dbglogfile.setWriteDisk(true);
    dbglogfile.setLogFilename("raw-test.log");
    dbglogfile.setVerbosity(3);

    const std::string dbconn{getenv("UNDERPASS_TEST_DB_CONN")
                                    ? getenv("UNDERPASS_TEST_DB_CONN")
                                    : "user=underpass_test host=localhost password=underpass_test"};

    TestPlanet test_planet;
    test_planet.init_test_case(dbconn);
    auto db = std::make_shared<Pq>();

    if (db->connect(dbconn + " dbname=underpass_test")) {
        auto queryraw = std::make_shared<QueryRaw>(db);
        std::map<long, std::shared_ptr<osmobjects::OsmWay>> waycache;
        std::string waysIds;

        processFile("raw-case-1.osc", db);
        processFile("raw-case-2.osc", db);

        for (auto const& x : expectedGeometries) {
            waysIds += std::to_string(x.first) + ",";
        }
        waysIds.erase(waysIds.size() - 1);

        queryraw->getWaysByIds(waysIds, waycache);

        // 4 created Nodes, 1 created Way (same changeset)
        if ( getWKT(waycache.at(101874)->polygon).compare(expectedGeometries.at(101874)) == 0) {
            runtest.pass("4 created Nodes, 1 created Ways (same changeset)");
        } else {
            runtest.fail("4 created Nodes, 1 created Ways (same changeset)");
            return 1;
        }

        // 1 created Way, 4 existing Nodes (different changeset)
        if ( getWKT(waycache.at(101875)->polygon).compare(expectedGeometries.at(101875)) == 0) {
            runtest.pass("1 created Way, 4 existing Nodes (different changesets)");
        } else {
            runtest.fail("1 created Way, 4 existing Nodes (different changesets)");
            return 1;
        }

        // 1 modified node, indirectly modify other existing ways
        processFile("raw-case-3.osc", db);
        waycache.erase(101875);
        queryraw->getWaysByIds(waysIds, waycache);

        if ( getWKT(waycache.at(101875)->polygon).compare(expectedGeometries.at(101875-2)) == 0) {
            runtest.pass("1 modified Node, indirectly modify other existing Ways (different changesets)");
        } else {
            runtest.fail("1 modified Node, indirectly modify other existing Ways (different changesets)");
            return 1;
        }

        // 1 created Relation referencing 1 created Way and 1 existing Way
        processFile("raw-case-4.osc", db);
        if ( getWKTFromDB("relations", 211766, db).compare(expectedGeometries.at(211766)) == 0) {
            runtest.pass("1 created Relation referencing 1 created Way and 1 existing Way (different changesets)");
        } else {
            runtest.fail("1 created Relation referencing 1 created Way and 1 existing Way (different changesets)");
            return 1;
        }

        // 1 modified Node, indirectly modify other existing Ways and 1 Relation
        processFile("raw-case-5.osc", db);

        if ( getWKTFromDB("relations", 211766, db).compare(expectedGeometries.at(211766-2)) == 0) {
            runtest.pass("1 modified Node, indirectly modify other existing Ways and 1 Relation (different changesets)");
        } else {
            runtest.fail("1 modified Node, indirectly modify other existing Ways and 1 Relation (different changesets)");
            return 1;
        }

        // 4 created Nodes, 2 created Ways, 1 created Relation with type=multilinestring
        processFile("raw-case-6.osc", db);
        if ( getWKTFromDB("relations", 211776, db).compare(expectedGeometries.at(211776)) == 0) {
            runtest.pass("4 created Nodes, 2 created Ways, 1 created Relation with type=multilinestring (same changeset)");
        } else {
            runtest.fail("4 created Nodes, 2 created Ways, 1 created Relation with type=multilinestring (same changeset)");
            return 1;
        }

    }



}