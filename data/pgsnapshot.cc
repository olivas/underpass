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

// The Dump handler
#include <osmium/handler/dump.hpp>
#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;

#include "hotosm.hh"
#include "data/pgsnapshot.hh"

namespace pgsnapshot {

// These tables are empty by default, and only used when applying a changeset
// pgsnapshot.state
// id | tstamp | sequence_number | state_timestamp | disabled 
//
// pgsnapshot.locked
// id | started | process | source | location | write_lock
//
// pgsnapshot.actions
// data_type | action | id
//
// pgsnapshot.replication_changes
// id | tstamp | nodes_modified | nodes_added | nodes_deleted | ways_modified | ways_added | ways_deleted | relations_modified | relations_added | relations_deleted | changesets_applied | earliest_timestamp | latest_timestamp
//
// pgsnapshot.sql_changes
// id | tstamp | entity_id | type | changeset_id | change_time | action | query | arguments
bool
PGSnapshot::connect(const std::string &dbname, const std::string &server)
{
    std::string args;
    if (dbname.empty()) {
	args = "dbname = pgsnapshot";
    } else {
	args = "dbname = " + dbname;
    }
    
    try {
	db = new pqxx::connection(args);
	if (db->is_open()) {
	    return true;
	} else {
	    return false;
	}
    } catch (const std::exception &e) {
	std::cerr << e.what() << std::endl;
	return false;
   }    
}


// pgsnapshot.users
// id | name
bool
PGSnapshot::addUser(long uid, const std::string &user)
{
    std::string query = "INSERT INTO users VALUES(";
    query += std::to_string(uid) + ",\'" + user;
    query += "\') ON CONFLICT DO NOTHING;";
    worker = new pqxx::work(*db);
    pqxx::result result = worker->exec(query);
    worker->commit();

    // FIXME: this should return a real value
    return false;
}

// pgsnapshot.nodes
// id | version | user_id | tstamp | changeset_id | tags | geom
bool
PGSnapshot::addNode(const osmium::Node& node)
{
    std::string query = "INSERT INTO nodes(id, version, user_id, tstamp, changeset_id, tags, geom) VALUES(";
    query += std::to_string(node.id()) + ",";
    query += + node.version();
    query += "," + std::to_string(node.uid()) + ",\'" + node.user() + "\'";
    query += ",\'" + node.timestamp().to_iso() + "\'";
    query += "\'," + node.changeset();
    query += "\'," + node.location().x();
    query += ") ON CONFLICT DO NOTHING;";
    std::cout << "Query: " << query << std::endl;
    
    worker = new pqxx::work(*db);
    pqxx::result result = worker->exec(query);
    worker->commit();

    // FIXME: this should return a real value
    return false;
}

// pgsnapshot.ways
// id | version | user_id | tstamp | changeset_id | tags | nodes | bbox | linestring
//
// pgsnapshot.way_nodes
// way_id | node_id | sequence_id
bool
PGSnapshot::addWay(const osmium::Way& way)
{
    std::string query = "INSERT INTO ways(id,version,user_id,tstamp,changeset_id,tags,nodes,bbox,linestring) VALUES(";
    query += std::to_string(way.id()) + "," + std::to_string(way.version());
    query += "\') ON CONFLICT DO NOTHING;";
    query += "," + std::to_string(way.uid()) + ",\'" + way.user() + "\'";
    query += ",\'" + way.timestamp().to_iso() + "\'";
    query += "\'," + way.changeset();

    worker = new pqxx::work(*db);
    pqxx::result result = worker->exec(query);
    int i = 0;
    for (const osmium::NodeRef& nref : way.nodes()) {
        std::cout << "ref:  " << nref.ref()
                  << std::endl;
    }
    
    // for (const osmium::Tag& t : way.tags()) {
    //     std::cout << "\t" << t.key() << "=" << t.value() << std::endl;
    //     query = "INSERT INTO way_nodes(way_id,node_id,sequence_id) VALUES(";
    //     query += std::to_string(way.id()) + ",");
    //     query += "\') ON CONFLICT DO NOTHING;";
    // }
    
    
    worker->commit();

    // FIXME: this should return a real value
    return false;
}

// pgsnapshot.relations
// id | version | user_id | tstamp | changeset_id | tags
//
// pgsnapshot.relation_members
// relation_id | member_id | member_type | member_role | sequence_id
bool
PGSnapshot::addRelation(const osmium::Relation& relation)
{

}

}       // EOF pgsnapshot

