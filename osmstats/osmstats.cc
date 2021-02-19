//
// Copyright (c) 2020, 2021 Humanitarian OpenStreetMap Team
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
#include "pqxx/nontransaction"

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;

#include "osmstats/osmstats.hh"
#include "osmstats/changeset.hh"
#include "data/geoutil.hh"
#include "data/underpass.hh"

using namespace apidb;

/// \namespace osmstats
namespace osmstats {

QueryOSMStats::QueryOSMStats(void)
{
}

bool
QueryOSMStats::connect(const std::string &dbname)
{
    std::string args;
    if (dbname.empty()) {
	args = "dbname = osmstats";
    } else {
	args = "dbname = " + dbname;
    }
    
    try {
	sdb = std::make_shared<pqxx::connection>(args);
	if (sdb->is_open()) {
	    return true;
	} else {
	    return false;
	}
    } catch (const std::exception &e) {
	std::cerr << e.what() << std::endl;
	return false;
   }    
}

// long
// QueryOSMStats::updateCounters(std::map<const std::string &, long> data)
// {
//     for (auto it = std::begin(data); it != std::end(data); ++it) {
//     }
// }

int
QueryOSMStats::lookupHashtag(const std::string &hashtag)
{
    std::string query = "SELECT id FROM taw_hashtags WHERE hashtag=\'";
    query += hashtag + "\';";
    pqxx::work worker(*sdb);
    pqxx::result result = worker.exec(query);
    worker.commit();

    // There is only one value returned
    return result[0][0].as(int(0));
}

bool
QueryOSMStats::applyChange(osmchange::ChangeStats &change)
{
    std::cout << "Applying OsmChange data" << std::endl;

    if (hasHashtag(change.change_id)) {
        std::cout << "Has hashtag for id: " << change.change_id << std::endl;
    } else {
        std::cerr << "No hashtag for id: " << change.change_id << std::endl;
    }
    std::string query = "UPDATE raw_changesets SET id=";
    query += std::to_string(change.change_id);
    query += ", road_km_added=" + std::to_string(change.roads_km_added);
    query += ", road_km_modified=" + std::to_string(change.roads_km_modified);
    query += ", waterway_km_added=" + std::to_string(change.waterways_km_added);
    query += ", waterway_km_modified=" + std::to_string(change.waterways_km_modified);
    query += ", roads_added=" + std::to_string(change.roads_added);
    query += ", roads_modified=" + std::to_string(change.roads_modified);
    query += ", waterways_added=" + std::to_string(change.waterways_added);
    query += ", waterways_modified=" + std::to_string(change.waterways_modified);
    query += ", buildings_added=" + std::to_string(change.buildings_added);
    query += ", buildings_modified=" + std::to_string(change.buildings_modified);
    query += ", pois_added=" + std::to_string(change.pois_added);
    query += ", pois_modified=" + std::to_string(change.pois_modified);
    ptime now = boost::posix_time::microsec_clock::local_time();
    query += ", updated_at=\'" + to_simple_string(now) + "\'";
    query += " WHERE id=" + std::to_string(change.change_id) + ";";
    // boost::algorithm::replace_all(change.user, "\'", "&quot;");
    // query += std::to_string(change.uid) + ",\'" + change.user;
    //query += "\') ON CONFLICT DO NOTHING;";
    std::cout << "QUERY: " << query << std::endl;
    pqxx::work worker(*sdb);
    pqxx::result result = worker.exec(query);

    worker.commit();
}

bool
QueryOSMStats::applyChange(changeset::ChangeSet &change)
{
    std::cout << "Applying ChangeSet data" << std::endl;

    change.dump();
    // Add user data
    // addUser(change.uid, change.user);
    std::string query = "INSERT INTO raw_users VALUES(";
    boost::algorithm::replace_all(change.user, "\'", "&quot;");
    query += std::to_string(change.uid) + ",\'" + change.user;
    query += "\') ON CONFLICT DO NOTHING;";
    std::cout << "QUERY: " << query << std::endl;
    pqxx::work worker(*sdb);
    pqxx::result result = worker.exec(query);
    //worker.commit();

    // If there are no hashtags in this changset, then it isn't part
    // of an organized map campaign, so we don't need to store those
    // statistics except for editor usage.
    underpass::Underpass under;
    if (change.hashtags.size() == 0) {
        std::cout << "No hashtags in change id: " << change.id << std::endl;
        under.updateCreator(change.uid, change.id, change.editor);
        worker.commit();
        return true;
    } else {
        std::cout << "Found hashtags for change id: " << change.id << std::endl;
    }
    //pqxx::work worker2(*sdb);
    for (auto it = std::begin(change.hashtags); it != std::end(change.hashtags); ++it) {
        query = "INSERT INTO raw_hashtags (hashtag) VALUES(\'";
        query += *it + "\') ON CONFLICT DO NOTHING;";
        std::cout << "QUERY: " << query << std::endl;
        pqxx::result result = worker.exec(query);
        std::string query = "SELECT id FROM raw_hashtags WHERE hashtag=\'" + *it + "\';";
        std::cout << "QUERY: " << query << std::endl;
        result = worker.exec(query);
        std::string id = result[0][0].c_str();
        query = "INSERT INTO raw_changesets_hashtags(changeset_id,hashtag_id) VALUES(";
        query += std::to_string(change.id) + ", " + id + ") ON CONFLICT DO NOTHING;";
        std::cout << "QUERY: " << query << std::endl;
        result = worker.exec(query);
    }

    // Update the raw_changesets_countries table
    auto country = under.getCountry(change.max_lat, change.max_lon,
                                                    change.min_lat, change.min_lon);
    if (country->id > 0) {
        query = "INSERT INTO raw_changesets_countries(changeset_id, country_id) VALUES(";
        query += std::to_string(change.id) + ", " + std::to_string(country->id);
        query += ") ON CONFLICT DO NOTHING;";
        std::cout << "QUERY: " << query << std::endl;
        result = worker.exec(query);
    } else {
        std::cerr << "ERROR: Can't find country for change ID: " << change.id << std::endl;
    }
    //worker2.commit();
    // Add changeset data
    // road_km_added | road_km_modified | waterway_km_added | waterway_km_modified | roads_added | roads_modified | waterways_added | waterways_modified | buildings_added | buildings_modified | pois_added | pois_modified | verified | augmented_diffs | updated_at
    // the updated_at timestamp is set after the change data has been processed

    // osmstats=# UPDATE raw_changesets SET road_km_added = (SELECT road_km_added + 10.0 FROM raw_changesets WHERE road_km_added>0 AND user_id=649260 LIMIT 1) WHERE user_id=649260;

    if (!change.open) {
        query = "INSERT INTO raw_changesets (id, editor, user_id, created_at, closed_at) VALUES(";
    } else {
        query = "INSERT INTO raw_changesets (id, editor, user_id, created_at) VALUES(";
    }
    boost::algorithm::replace_all(change.editor, "\'", "&quot;");
    query += std::to_string(change.id) + ",\'" + change.editor + "\',\'";\
    query += std::to_string(change.uid) + "\',\'";
    query += to_simple_string(change.created_at) + "\'";
    if (!change.open) {
        query += ",\'" + to_simple_string(change.closed_at) + "\')";
    } else {
        query += ")";
    }
    query += " ON CONFLICT DO NOTHING;";
    std::cout << "QUERY: " << query << std::endl;
    //auto worker3 = std::make_shared<pqxx::work>(*db);
    result = worker.exec(query);

    // Commit the results to the database
    worker.commit();

    return true;
}

// Populate internal storage of a few heavily used data, namely
// the indexes for each user, country, or hashtag.
bool
QueryOSMStats::populate(void)
{
    // Get the country ID from the raw_countries table
    std::string query = "SELECT id,name,code FROM raw_countries;";
    pqxx::work worker(*db);
    pqxx::result result = worker.exec(query);
    for (auto it = std::begin(result); it != std::end(result); ++it) {
        RawCountry rc(it);
        // addCountry(rc);
        // long id = std::stol(it[0].c_str());
        countries.push_back(rc);
    };

    query = "SELECT id,name FROM raw_users;";
    pqxx::work worker2(*db);
    result = worker2.exec(query);
    for (auto it = std::begin(result); it != std::end(result); ++it) {
        RawUser ru(it);
        users.push_back(ru);
    };

    query = "SELECT id,hashtag FROM raw_hashtags;";
    result = worker.exec(query);
    for (auto it = std::begin(result); it != std::end(result); ++it) {
        RawHashtag rh(it);
        hashtags[rh.name] = rh;
    };

    // ptime start = time_from_string("2010-07-08 13:29:46");
    // ptime end = second_clock::local_time();
    // long roadsAdded = QueryStats::getCount(QueryStats::highway, 0,
    //                                        QueryStats::totals, start, end);
    // long roadKMAdded = QueryStats::getLength(QueryStats::highway, 0,
    //                                          start, end);
    // long waterwaysAdded = QueryStats::getCount(QueryStats::waterway, 0,
    //                                            QueryStats::totals, start, end);
    // long waterwaysKMAdded = QueryStats::getLength(QueryStats::waterway, 0,
    //                                               start, end);
    // long buildingsAdded = QueryStats::getCount(QueryStats::waterway, 0,
    //                                            QueryStats::totals, start, end);

    worker2.commit();

    return true;
};

bool
QueryOSMStats::getRawChangeSets(std::vector<long> &changeset_ids)
{
    pqxx::work worker(*db);
    std::string sql = "SELECT id,road_km_added,road_km_modified,waterway_km_added,waterway_km_modified,roads_added,roads_modified,waterways_added,waterways_modified,buildings_added,buildings_modified,pois_added,pois_modified,editor,user_id,created_at,closed_at,verified,augmented_diffs,updated_at FROM raw_changesets WHERE id=ANY(ARRAY[";
    // Build an array string of the IDs
    for (auto it = std::begin(changeset_ids); it != std::end(changeset_ids); ++it) {
        sql += std::to_string(*it);
        if (*it != changeset_ids.back()) {
            sql += ",";
        }
    }
    sql += "]);";

    std::cout << "QUERY: " << sql << std::endl;
    pqxx::result result = worker.exec(sql);
    std::cout << "SIZE: " << result.size() <<std::endl;
    //OsmStats stats(result);
    worker.commit();

    for (auto it = std::begin(result); it != std::end(result); ++it) {
        RawChangeset os(it);
        ostats.push_back(os);
    }

    return true;
}

bool
QueryOSMStats::hasHashtag(long changeid)
{
    std::string query = "SELECT COUNT(hashtag_id) FROM raw_changesets_hashtags WHERE changeset_id=" + std::to_string(changeid) + ";";
    std::cout << "QUERY: " << query << std::endl;
    pqxx::work worker(*sdb);
    pqxx::result result = worker.exec(query);
    worker.commit();

    if (result[0][0].as(int(0)) > 0 ) {
        return true;
    }

    return false;
}

// Get the timestamp of the last update in the database
ptime
QueryOSMStats::getLastUpdate(void)
{
    std::string query = "SELECT MAX(created_at) FROM raw_changesets;";
    std::cout << "QUERY: " << query << std::endl;
    // auto worker = std::make_shared<pqxx::work>(*db);
    pqxx::work worker(*sdb);
    pqxx::result result = worker.exec(query);
    worker.commit();

    ptime last;
    if (result[0][0].size() > 0) {
        last = time_from_string(result[0][0].c_str());
        return last;
    }
    return not_a_date_time;
}

bool
QueryOSMStats::updateCounters(long cid, std::map<std::string, long> data)
{
    std::string query = "UPDATE raw_changeset SET ";
    for (auto it = std::begin(data); it != std::end(data); ++it) {
        query += "," + it->first + "=" + std::to_string(it->second);
    }
    query += " WHERE id=" + std::to_string(cid);
    std::cout << "QUERY: " << query << std::endl;
}

void
QueryOSMStats::dump(void)
{
    for (auto it = std::begin(ostats); it != std::end(ostats); ++it) {
        it->dump();
    }
}

// Write the list of hashtags to the database
int
QueryOSMStats::updateRawHashtags(void)
{
//    INSERT INTO keys(name, value) SELECT 'blah', 'true' WHERE NOT EXISTS (SELECT 1 FROM keys WHERE name='blah');

    std::string query = "INSERT INTO raw_hashtags(hashtag) VALUES(";
    // query += "nextval('raw_hashtags_id_seq'), \'" + tag;
    // query += "\'" + tag;
    query += "\') ON CONFLICT DO NOTHING";
    std::cout << "QUERY: " << query << std::endl;
    pqxx::work worker(*db);
    pqxx::result result = worker.exec(query);
    worker.commit();
    return result.size();
}

int
QueryOSMStats::updateCountries(std::vector<RawCountry> &countries)
{
    pqxx::work worker(*db);
    for (auto it = std::begin(countries); it != std::end(countries); ++it) {
        std::string query = "INSERT INTO raw_countries VALUES";
        query += std::to_string(it->id) + ",\'" + it->name + "',\'" + it->abbrev + "\');";
        query += ") ON CONFLICT DO NOTHING";
        pqxx::result result = worker.exec(query);
    }
    worker.commit();
    return countries.size();
}

RawChangeset::RawChangeset(pqxx::const_result_iterator &res)
{
    id = std::stol(res[0].c_str());
    for (auto it = std::begin(res); it != std::end(res); ++it) {
        if (it->type() == 20 || it->type() == 23) {
            if (it->name() != "id" && it->name() != "user_id") {
                counters[it->name()] = it->as(long(0));
            }
            // FIXME: why are there doubles in the schema at all ? It's
            // a counter, so should always be an integer or long
        } else if (it->type() == 701) { // double
            counters[it->name()] = it->as(double(0));
        }
    }
    editor = pqxx::to_string(res[13]);
    user_id = std::stol(res[14].c_str());
    created_at = time_from_string(pqxx::to_string(res[15]));
    closed_at = time_from_string(pqxx::to_string(res[16]));
    // verified = res[17].bool();
    // augmented_diffs = res[18].num();
    updated_at = time_from_string(pqxx::to_string(res[19]));
}

void
RawChangeset::dump(void)
{
    std::cout << "-----------------------------------" << std::endl;
    std::cout << "changeset id: \t\t " << id << std::endl;
    std::cout << "Roads Added (km): \t " << counters["road_km_added"] << std::endl;
    std::cout << "Roads Modified (km):\t " << counters["road_km_modified"] << std::endl;
    std::cout << "Waterways Added (km): \t " << counters["waterway_km_added"] << std::endl;
    std::cout << "Waterways Modified (km): " << counters["waterway_km_modified"] << std::endl;
    std::cout << "Roads Added: \t\t " << counters["roads_added"] << std::endl;
    std::cout << "Roads Modified: \t " << counters["roads_modified"] << std::endl;
    std::cout << "Waterways Added: \t " << counters["waterways_added"] << std::endl;
    std::cout << "Waterways Modified: \t " << counters["waterways_modified"] << std::endl;
    std::cout << "Buildings added: \t " << counters["buildings_added"] << std::endl;
    std::cout << "Buildings Modified: \t " << counters["buildings_modified"] << std::endl;
    std::cout << "POIs added: \t\t " << counters["pois_added"] << std::endl;
    std::cout << "POIs Modified: \t\t " << counters["pois_modified"] << std::endl;
    std::cout << "Editor: \t\t " << editor << std::endl;
    std::cout << "User ID: \t\t "  << user_id << std::endl;
    std::cout << "Created At: \t\t " << created_at << std::endl;
    std::cout << "Closed At: \t\t " << closed_at << std::endl;
    std::cout << "Verified: \t\t " << verified << std::endl;
    // std::cout << augmented_diffs << std::endl;
    std::cout << "Updated At: \t\t " << updated_at << std::endl;
}

}       // EOF osmstatsdb

