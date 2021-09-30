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

#include <chrono>
#include <string>
#include <regex>

#include <pqxx/nontransaction>

#include <boost/iostreams/copy.hpp>
#include <boost/process.hpp>
using namespace boost::process;
#include <boost/format.hpp>
using boost::format;

#include <boost/algorithm/string.hpp>

#include "osm2pgsql.hh"
#include "data/osmobjects.hh"
using namespace osmobjects;

#include "log.hh"
using namespace logger;

namespace osm2pgsql {

const std::string Osm2Pgsql::OSM2PGSQL_DEFAULT_SCHEMA_NAME = "osm2pgsql_pgsql";

logger::LogFile &dbglogfile = logger::LogFile::getDefaultInstance();

const std::regex Osm2Pgsql::TagParser::tags_escape_re = std::regex(R"re(("))re");

// clang-format off
const std::set<std::string> Osm2Pgsql::TagParser::polygon_tags = {
    "aeroway",
    "amenity",
    "building",
    "harbour",
    "historic",
    "landuse",
    "leisure",
    "man_made",
    "military",
    "natural",
    "office",
    "place",
    "power",
    "public_transport",
    "shop",
    "sport",
    "tourism",
    "water",
    "waterway",
    "wetland"
};

// Process tags, this is the list of tag names that map to all table columns
const std::set<std::string> Osm2Pgsql::TagParser::column_stored_tags = {
    "access",
    "addr:housename",
    "addr:housenumber",
    "addr:interpolation",
    "admin_level",
    "aerialway",
    "aeroway",
    "amenity",
    "area",
    "barrier",
    "bicycle",
    "brand",
    "bridge",
    "boundary",
    "building",
    "construction",
    "covered",
    "culvert",
    "cutting",
    "denomination",
    "disused",
    "embankment",
    "foot",
    "generator:source",
    "harbour",
    "highway",
    "historic",
    "horse",
    "intermittent",
    "junction",
    "landuse",
    "layer",
    "leisure",
    "lock",
    "man_made",
    "military",
    "motorcar",
    "name",
    "natural",
    "office",
    "oneway",
    "operator",
    "place",
    "population",
    "power",
    "power_source",
    "public_transport",
    "railway",
    "ref",
    "religion",
    "route",
    "service",
    "shop",
    "sport",
    "surface",
    "toll",
    "tourism",
    "tower:type",
    "tunnel",
    "water",
    "waterway",
    "wetland",
    "width",
    "wood"
};

// Process tags, this is the list of additional tag names that map to points table columns only
const std::set<std::string> Osm2Pgsql::TagParser::column_points_stored_tags = {
    "capital",  // Points only
    "ele", // Points only
};


const std::map<std::pair<std::string, std::string>, std::pair<bool, int>> Osm2Pgsql::TagParser::z_index_map = {
    {{"railway", ""}, {5, 1}},
    {{"boundary", "administrative "}, {0, 1}},
    {{"bridge", "yes"}, {10, 0}},
    {{"bridge", "true"}, {10, 0}},
    {{"bridge", "1"}, {10, 0}},
    {{"tunnel", "yes"}, {-10, 0}},
    {{"tunnel", "true"}, {-10, 0}},
    {{"tunnel", "1"}, {-10, 0}},
    {{"highway", "minor"}, {3, 0}},
    {{"highway", "road"}, {3, 0}},
    {{"highway", "unclassified"}, {3, 0}},
    {{"highway", "residential"}, {3, 0}},
    {{"highway", "tertiary_link"}, {4, 0}},
    {{"highway", "tertiary"}, {4, 0}},
    {{"highway", "secondary_link"}, {6, 1}},
    {{"highway", "secondary"}, {6, 1}},
    {{"highway", "primary_link"}, {7, 1}},
    {{"highway", "primary"}, {7, 1}},
    {{"highway", "trunk_link"}, {8, 1}},
    {{"highway", "trunk"}, {8, 1}},
    {{"highway", "motorway_link"}, {9, 1}},
    {{"highway", "motorway"}, {9, 1}}
};

// clang-format on

Osm2Pgsql::Osm2Pgsql(const std::string &_dburl, const std::string &schema) : pq::Pq(), schema(schema)
{
    if (!connect(_dburl)) {
        log_error(_("Could not connect to osm2pgsql server %1%"), _dburl);
    }
}

ptime
Osm2Pgsql::getLastUpdate()
{
    if (last_update.is_not_a_date_time()) {
        getLastUpdateFromDb();
    }
    return last_update;
}

#if 0

// osm2pgsql fork is disabled but left in the sources because it may be useful to
// compare the results of the original application with those of our clone
// implementation.

bool
Osm2Pgsql::updateDatabase(const std::string &osm_changes)
{

    // -l for 4326
    std::string osm2pgsql_update_command{
        "osm2pgsql -l --append -r xml -s -C 300 -G --hstore-all --extra-attributes --middle-schema=" + schema +
        " --output-pgsql-schema=" + schema + " -d postgresql://"};
    // Append DB url in the form: postgresql://[user[:password]@][netloc][:port][,...][/dbname][?param1=value1&...]
    osm2pgsql_update_command.append(dburl);
    // Read from stdin
    osm2pgsql_update_command.append(" -");

    log_debug(_("Executing osm2pgsql, command: %1%"), osm2pgsql_update_command);

    ipstream out;
    ipstream err;
    opstream in;

    child osm2pgsql_update_process(osm2pgsql_update_command, std_out > out, std_err > err, std_in < in);

    in.write(osm_changes.data(), osm_changes.size());
    in.close();
    in.pipe().close();

    // FIXME: make wait_for duration an arg
    bool result{(!osm2pgsql_update_process.running() || osm2pgsql_update_process.wait_for(std::chrono::minutes{1})) &&
                osm2pgsql_update_process.exit_code() == EXIT_SUCCESS};
    if (!result) {
        std::stringstream err_mesg;
        boost::iostreams::copy(err, err_mesg);
        log_error(_("Error running osm2pgsql command %1%\n%2%"), osm2pgsql_update_command, err_mesg.str());
    }
    return result;
}
#endif

bool
Osm2Pgsql::updateDatabase(const std::shared_ptr<OsmChangeFile> &osm_changes) const
{
    // Preconditions
    assert(static_cast<bool>(osm_changes));

    if (!isOpen()) {
        log_error(_("Could not update osm2pgsql DB %1%: DB is closed!"), dburl);
        return false;
    }

    for (const auto &change: std::as_const(osm_changes->changes)) {
        switch (change->action) {
            case action_t::modify:
            case action_t::create:
            {
                for (const auto &node: std::as_const(change->nodes)) {
                    upsertNode(node);
                }
                for (const auto &way: std::as_const(change->ways)) {
                    upsertWay(way);
                }
                for (const auto &relation: std::as_const(change->relations)) {
                    upsertRelation(relation);
                }
                break;
            }
            case action_t::remove:
            {
                for (const auto &node: std::as_const(change->nodes)) {
                    removeNode(node);
                }
                for (const auto &way: std::as_const(change->ways)) {
                    removeWay(way);
                }
                for (const auto &relation: std::as_const(change->relations)) {
                    removeRelation(relation);
                }
                break;
            }
            case action_t::none:
            {
                break;
            }
        }
    }

    return true;
}

bool
Osm2Pgsql::upsertWay(const std::shared_ptr<osmobjects::OsmWay> &way) const
{
    // Preconditions
    assert(static_cast<bool>(sdb));

    TagParser parser;
    {
        pqxx::nontransaction worker(*sdb);
        auto tags{way->tags};
        tags.emplace("osm_version", std::to_string(way->version));
        tags.emplace("osm_changeset", std::to_string(way->change_id));
        tags.emplace("osm_timestamp", to_iso_extended_string(way->timestamp) + "Z");
        parser.parse(tags, worker, false);
    }

    std::string refs{"'{"};
    bool is_first{true};
    for (const auto &ref: std::as_const(way->refs)) {
        if (is_first) {
            is_first = false;
        } else {
            refs.append(",");
        }
        refs.append(std::to_string(ref));
    }
    refs.append("}'");

    // First: upsert in middle table
    const std::string middle_sql{str(format(R"sql(
      INSERT INTO %1%.planet_osm_ways
        (id, nodes, tags)
        VALUES ($1, %2%, %3%)
      ON CONFLICT (id) DO
        UPDATE
        SET nodes = %2%, tags = %3%
        WHERE %1%.planet_osm_ways.id = $1
      )sql") % schema % refs % parser.tags_array_literal)};

    // No upsert here because planet_osm_roads/line have no PK ...
    const std::string delete_roads_sql{str(format("DELETE FROM %1%.planet_osm_roads WHERE osm_id = $1") % schema)};
    const std::string delete_line_sql{str(format("DELETE FROM %1%.planet_osm_line WHERE osm_id = $1") % schema)};
    const std::string delete_polygon_sql{str(format("DELETE FROM %1%.planet_osm_polygon WHERE osm_id = $1") % schema)};
    const std::string update_way_area_sql{
        str(format("UPDATE %1%.planet_osm_polygon SET way_area = ST_Area(way) WHERE osm_id = $1") % schema)};

    std::string insert_sql;

    if (parser.is_polygon && !way->isClosed()) {
        // This is not necessarily an error: some tags might be applied to both polygons and linestrings
        log_debug(_("Way %1% is tagged as a possible polygon but it isn't closed! Storing as linestring."), way->id);
    }

    if (parser.is_polygon && way->isClosed()) {
        // Insert
        insert_sql = str(format(R"sql(
          INSERT INTO %1%.planet_osm_%2%
            (osm_id, way, tags %3%)
          VALUES ($1, ST_SetSRID( ST_MakePolygon( ST_MakeLine( ARRAY(
            SELECT ST_MakePoint(n.lon/10000000.0 , n.lat/10000000.0) FROM %1%.planet_osm_nodes n
            JOIN UNNEST(%6%::bigint[]) WITH ORDINALITY t(id, ord) USING (id) ORDER BY t.ord ))), 4326 ),
          %5% %4%)
        )sql") % schema % "polygon" %
                         parser.tag_field_names % parser.tag_field_values % parser.tags_hstore_literal % refs);

    } else {
        // Decide if it's a road or a line
        const std::string way_type{parser.is_road ? "roads" : "line"};

        // Insert
        insert_sql = str(format(R"sql(
          INSERT INTO %1%.planet_osm_%2%
            (osm_id, way, tags %3%)
          VALUES ($1, ST_SetSRID( ST_MakeLine( ARRAY(
            SELECT ST_MakePoint(n.lon/10000000.0 , n.lat/10000000.0) FROM %1%.planet_osm_nodes n
            JOIN UNNEST(%6%::bigint[]) WITH ORDINALITY t(id, ord) USING (id) ORDER BY t.ord )), 4326 ),
          %5% %4%)
          )sql") % schema %
                         way_type % parser.tag_field_names % parser.tag_field_values % parser.tags_hstore_literal % refs);
    }

    // Collect multipolygons that need update
    const std::string polygons_sql{str(format(R"sql(
      SELECT - id AS id, members, parts FROM %1%.planet_osm_rels
        WHERE $1 = ANY (parts) AND hstore(tags) -> 'type' = 'multipolygon'
       )sql") % schema)};

    pqxx::work worker(*sdb);

    const auto results{worker.exec_params(polygons_sql, way->id)};

    std::map<long, std::list<Polygon>> polygons_map;

    for (const auto &res: results) {
        const std::string parts{res["parts"].as(std::string())};
        const std::string members{res["members"].as(std::string())};
        std::list<Polygon> polygons;
        // Unsure if a multipolygon can have a single outer ring and no inner rings, assuming NOT
        if (parts.size() > 2) {
            std::vector<std::string> members_list;
            boost::split(members_list, members.substr(1, members.size() - 2), [](char c) { return c == ','; });
            std::vector<std::string> parts_list;
            boost::split(parts_list, parts.substr(1, parts.size() - 2), [](char c) { return c == ','; });
            if (parts_list.size() > 1 && members_list.size() == 2 * parts_list.size()) {
                for (int idx = 0; idx < parts_list.size(); ++idx) {
                    try {
                        const auto way_id{std::stol(parts_list[idx])};
                        const auto role{members_list[idx * 2 + 1]};
                        if (role == "inner") {
                            // Found in the wild a polygon where inner rings are listed before outer rings
                            // In this case we create a polygon with no outer ring, hoping that we'll get it later
                            if (polygons.size() == 0) {
                                Polygon poly{};
                                polygons.push_back(Polygon());
                                polygons.back().id = res["id"].as(long());
                            }
                            if (!polygons.back().inner.empty()) {
                                polygons.back().inner.push_back(',');
                            }
                            polygons.back().inner.append(std::to_string(way_id));
                        } else if (role == "outer") {
                            // Handle the case where a polygon with inner rings but without outer ring was created
                            if (polygons.size() > 0 && polygons.back().outer == std::numeric_limits<long>::lowest()) {
                                polygons.back().outer = way_id;
                            } else {
                                polygons.push_back(Polygon(way_id));
                                polygons.back().id = res["id"].as(long());
                            }
                        }
                    } catch (const std::exception &) {
                        log_error(_("Error parsing parts from relation %1%."), res["id"].as(std::string()));
                    }
                }
            } else {
                log_error(_("Size mismatch error parsing parts and members from multipolygon relation %1%."),
                          res["id"].as(std::string()));
            }
        }
        if (polygons.size() > 0) {
            const long poly_id{res["id"].as(long())};
            polygons_map.emplace(poly_id, polygons);
        }
    }

    worker.commit();

    pqxx::nontransaction worker_nt(*sdb);
    //std::cerr << middle_sql << std::endl;
    //std::cerr << insert_sql << std::endl;

    try {
        worker_nt.exec("BEGIN;");
        worker_nt.exec_params0(middle_sql, way->id);
        worker_nt.exec_params0(delete_line_sql, way->id);
        worker_nt.exec_params0(delete_roads_sql, way->id);
        worker_nt.exec_params0(delete_polygon_sql, way->id);
        worker_nt.exec_params0(insert_sql, way->id);

        if (parser.is_polygon && way->isClosed()) {
            worker_nt.exec_params0(update_way_area_sql, way->id);
        }

        // Update multi polygons

        for (auto poly_it = polygons_map.cbegin(); poly_it != polygons_map.cend(); ++poly_it) {
            std::string multi_polygons_parts_sql;
            for (const auto &poly: std::as_const(poly_it->second)) {
                const auto inner_id_sql{poly.inner.empty() ? std::to_string(std::numeric_limits<long>::lowest()) : poly.inner};

                if (poly.outer == std::numeric_limits<long>::lowest()) {
                    log_error(_("A polygon with no outer rings is invalid! Skipping relation %1%."), poly.id);
                } else {
                    if (!multi_polygons_parts_sql.empty()) {
                        multi_polygons_parts_sql.append(", ");
                    }
                    multi_polygons_parts_sql.append(str(format(R"sql(
               (SELECT ST_MakePolygon( ST_SetSRID(ST_MakeLine( ARRAY(
               SELECT ST_MakePoint(n.lon/10000000.0 , n.lat/10000000.0)
                 FROM %1%.planet_osm_nodes n
                 JOIN UNNEST(w.nodes)
                 WITH ORDINALITY t(id, ord) USING (id) ORDER BY t.ord )), 4326),
               ARRAY(SELECT ST_ExteriorRing(p.way) FROM %1%.planet_osm_polygon p WHERE p.osm_id IN(%3%)))
               FROM %1%.planet_osm_ways w WHERE w.id = %2%)
               )sql") % schema % poly.outer % inner_id_sql));
                }
            }

            if (!multi_polygons_parts_sql.empty()) {
                const auto polygon_update_sql{str(format(R"sql(
           UPDATE %1%.planet_osm_polygon SET way = ST_Collect(ARRAY[%3%])
           WHERE osm_id = %2%
         )sql") % schema % poly_it->first % multi_polygons_parts_sql)};

                //std::cerr << polygon_update_sql << std::endl;

                const auto results{worker_nt.exec0(polygon_update_sql)};

                if (results.affected_rows() > 0) {
                    const auto update_sql = str(format(R"sql(
               UPDATE %1%.planet_osm_polygon SET
                 way_area = ST_Area(way)
               WHERE osm_id = %2%)sql") % schema %
                                                poly_it->first);
                    worker_nt.exec0(update_sql);
                }

            } // iterator end
        }

        worker_nt.exec("COMMIT;");
    } catch (const std::exception &ex) {
        log_error(_("Couldn't upsert way/road record: %1%"), ex.what());
        worker_nt.exec("ROLLBACK;");
        return false;
    }

    return true;
}

bool
Osm2Pgsql::upsertNode(const std::shared_ptr<osmobjects::OsmNode> &node) const
{
    // Preconditions
    assert(static_cast<bool>(sdb));

    pqxx::nontransaction worker(*sdb);

    // First: upsert in middle table
    const std::string middle_sql{str(format(R"sql(
      INSERT INTO %1%.planet_osm_nodes
        (id, lon, lat)
        VALUES ($1, $2, $3)
      ON CONFLICT (id) DO
        UPDATE
        SET lon = $2, lat = $3
        WHERE %1%.planet_osm_nodes.id = $1
      )sql") % schema)};

    TagParser parser;
    auto tags{node->tags};
    tags.emplace("osm_version", std::to_string(node->version));
    tags.emplace("osm_changeset", std::to_string(node->change_id));
    tags.emplace("osm_timestamp", to_iso_extended_string(node->timestamp) + "Z");
    parser.parse(tags, worker, true);

    // No upsert here because planet_osm_point has no PK ...
    const std::string delete_sql{str(format(R"sql(DELETE FROM %1%.planet_osm_point WHERE osm_id = $1)sql") % schema)};
    // Insert
    const std::string insert_sql{str(format(R"sql(
  INSERT INTO %1%.planet_osm_point
    (osm_id, way, tags %2%)
    VALUES ($1, public.ST_SetSRID(public.ST_MakePoint($2, $3), 4326), %4% %3%)
  )sql") % schema % parser.tag_field_names %
                                     parser.tag_field_values % parser.tags_hstore_literal)};

    //std::cerr << middle_sql << std::endl;
    //std::cerr << insert_sql << std::endl;

    try {
        worker.exec("BEGIN;");
        worker.exec_params0(middle_sql, node->id, static_cast<int>(node->point.x() * 10000000),
                            static_cast<int>(node->point.y() * 10000000));
        worker.exec_params0(delete_sql, node->id);
        worker.exec_params0(insert_sql, node->id, node->point.x(), node->point.y());
        worker.exec("COMMIT;");
    } catch (const std::exception &ex) {
        log_error(_("Couldn't upsert node/points record: %1%"), ex.what());
        worker.exec("ROLLBACK;");
        return false;
    }
    return true;
}

bool
Osm2Pgsql::upsertRelation(const std::shared_ptr<osmobjects::OsmRelation> &relation) const
{

    pqxx::nontransaction worker(*sdb);

    TagParser parser;
    auto tags{relation->tags};
    tags.emplace("osm_version", std::to_string(relation->version));
    tags.emplace("osm_changeset", std::to_string(relation->change_id));
    tags.emplace("osm_timestamp", to_iso_extended_string(relation->timestamp) + "Z");
    parser.parse(tags, worker, false);

    // Multipolygons
    const bool is_multi_polygon{relation->tags.find("type") != relation->tags.end() && relation->tags.at("type") == "multipolygon"};

    std::list<Polygon> polygons;

    // First: upsert in middle table
    // The member attribute is an array with IDs, where all node members come first,
    // then all way members, then all relation members, and way_off is the index of the
    // first way member and rel_off the index of the first relation member.
    /*
   Column  |   Type   | Collation | Nullable | Default
  ---------+----------+-----------+----------+---------
   id      | bigint   |           | not null |
   way_off | smallint |           |          |
   rel_off | smallint |           |          |
   parts   | bigint[] |           |          |
   members | text[]   |           |          |
   tags    | text[]   |           |          |
 */

    std::string members{"'{"};
    std::string parts{"'{"};

    // Reorder members
    std::list<const osmobjects::OsmRelationMember *> nodes;
    std::list<const osmobjects::OsmRelationMember *> ways;
    std::list<const osmobjects::OsmRelationMember *> relations;
    for (const auto &rel: std::as_const(relation->members)) {
        switch (rel.type) {
            case osmobjects::osmtype_t::way:
            {
                ways.push_back(&rel);
                break;
            }
            case osmobjects::osmtype_t::node:
            {
                nodes.push_back(&rel);
                break;
            }
            case osmobjects::osmtype_t::relation:
            {
                relations.push_back(&rel);
                break;
            }
            case osmobjects::osmtype_t::empty:
            case osmobjects::osmtype_t::member:
                break;
        }
    };

    int way_off = nodes.size();
    int rel_off = nodes.size() + ways.size();

    std::list<const osmobjects::OsmRelationMember *> all_members;
    all_members.insert(all_members.end(), nodes.begin(), nodes.end());
    all_members.insert(all_members.end(), ways.begin(), ways.end());
    all_members.insert(all_members.end(), relations.begin(), relations.end());

    for (const auto &rel: std::as_const(all_members)) {
        if (members.size() > 2) {
            members.push_back(',');
        }
        switch (rel->type) {
            case osmobjects::osmtype_t::way:
            {
                members.append("w" + std::to_string(rel->ref) + ",\"" + rel->role + "\"");
                if (rel->role == "inner") {
                    // Found in the wild a polygon where inner rings are listed before outer rings
                    // In this case we create a polygon with no outer ring, hoping that we'll get it later
                    if (polygons.size() == 0) {
                        Polygon poly{};
                        polygons.push_back(Polygon());
                    }
                    if (!polygons.back().inner.empty()) {
                        polygons.back().inner.push_back(',');
                    }
                    polygons.back().inner.append(std::to_string(rel->ref));
                } else if (rel->role == "outer") {
                    // Handle the case where a polygon with inner rings but without outer ring was created
                    if (polygons.size() > 0 && polygons.back().outer == std::numeric_limits<long>::lowest()) {
                        polygons.back().outer = rel->ref;
                    } else {
                        polygons.push_back(Polygon(rel->ref));
                    }
                }
                break;
            }
            case osmobjects::osmtype_t::relation:
            {
                members.append("w" + std::to_string(rel->ref) + ",\"" + rel->role + "\"");
                break;
            }
            case osmobjects::osmtype_t::node:
            {
                members.append("w" + std::to_string(rel->ref) + ",\"" + rel->role + "\"");
                break;
            }
            case osmobjects::osmtype_t::empty:
            case osmobjects::osmtype_t::member:
                break;
        }

        // Store part id
        if (rel->type != osmobjects::osmtype_t::empty && rel->type != osmobjects::osmtype_t::member) {
            if (parts.size() > 2) {
                parts.push_back(',');
            }
            parts.append(std::to_string(rel->ref));
        }
    }

    members.append("}'");
    parts.append("}'");

    const std::string middle_sql{str(format(R"sql(
    INSERT INTO %1%.planet_osm_rels
      (id, way_off, rel_off, parts, members, tags)
      VALUES ($1, %2%, %3%, %4%, %5%, %6%)
    ON CONFLICT (id) DO
      UPDATE
      SET
        way_off = %2%,
        rel_off = %3%,
        parts  = %4%,
        members = %5%,
        tags = %6%
      WHERE %1%.planet_osm_rels.id = $1
    )sql") % schema % way_off % rel_off %
                                     parts % members % parser.tags_array_literal)};

    //std::cerr << middle_sql << std::endl;

    try {
        worker.exec("BEGIN;");
        worker.exec_params0(middle_sql, relation->id);
        worker.exec0("DELETE FROM " + schema + ".planet_osm_polygon WHERE osm_id = -" + std::to_string(relation->id));
        std::string multi_polygons_parts_sql;
        if (is_multi_polygon) {
            for (const auto &poly: std::as_const(polygons)) {
                const auto inner_id_sql{poly.inner.empty() ? std::to_string(std::numeric_limits<long>::lowest()) : poly.inner};
                if (poly.outer == std::numeric_limits<long>::lowest()) {
                    log_error(_("A polygon with no outer rings is invalid! Skipping relation %1%."), relation->id);
                } else {
                    if (!multi_polygons_parts_sql.empty()) {
                        multi_polygons_parts_sql.append(", ");
                    }
                    multi_polygons_parts_sql.append(str(format(R"sql(
                  (SELECT ST_MakePolygon( ST_SetSRID(ST_MakeLine( ARRAY(
                  SELECT ST_MakePoint(n.lon/10000000.0 , n.lat/10000000.0)
                    FROM %1%.planet_osm_nodes n
                    JOIN UNNEST(w.nodes)
                    WITH ORDINALITY t(id, ord) USING (id) ORDER BY t.ord )), 4326),
                  ARRAY(SELECT ST_ExteriorRing(p.way) FROM %1%.planet_osm_polygon p WHERE p.osm_id IN(%4%)))
                  FROM %1%.planet_osm_ways w WHERE w.id = %3%)
                  )sql") % schema % relation->id % poly.outer %
                                                        inner_id_sql));
                }
            }

            const auto insert_sql{str(format(R"sql(
             INSERT INTO %1%.planet_osm_polygon (osm_id, way, tags %3% )
                 VALUES (-%2%, ST_Collect(ARRAY[%6%]), %5% %4%)
             )sql") % schema % relation->id %
                                      parser.tag_field_names % parser.tag_field_values % parser.tags_hstore_literal %
                                      multi_polygons_parts_sql)};

            //std::cerr << insert_sql << std::endl;

            worker.exec0(insert_sql);

            // Update area
            const auto update_sql = str(format(R"sql(
               UPDATE %1%.planet_osm_polygon SET
                 way_area = ST_Area(way)
               WHERE osm_id = -%2%)sql") %
                                        schema % relation->id);
            worker.exec0(update_sql);

            //std::cerr << update_sql << std::endl;
        }
        worker.exec("COMMIT;");
    } catch (const std::exception &ex) {
        log_error(_("Couldn't upsert polygon records: %1%"), ex.what());
        worker.exec("ROLLBACK;");
        return false;
    }
    return true;
}

bool
Osm2Pgsql::removeWay(const std::shared_ptr<osmobjects::OsmWay> &way) const
{
    // Preconditions
    assert(static_cast<bool>(sdb));

    return true;
}

bool
Osm2Pgsql::removeNode(const std::shared_ptr<osmobjects::OsmNode> &node) const
{
    // Preconditions
    assert(static_cast<bool>(sdb));

    // upsert in middle nodes table
    const std::string middle_sql{str(format(R"sql(
  DELETE FROM %1%.planet_osm_nodes WHERE id = $1
  )sql") % schema)};
    // upsert in point table
    const std::string sql{str(format(R"sql(
  DELETE FROM %1%.planet_osm_point WHERE osm_id = $1
  )sql") % schema)};
    try {
        pqxx::work worker(*sdb);
        worker.exec_params(middle_sql, node->id);
        worker.exec_params(sql, node->id);
        worker.commit();
    } catch (const std::exception &ex) {
        log_error(_("Couldn't remove node record: %1%"), ex.what());
        return false;
    }
    return true;
}

bool
Osm2Pgsql::removeRelation(const std::shared_ptr<osmobjects::OsmRelation> &relation) const
{
    return true;
}

bool
Osm2Pgsql::connect(const std::string &_dburl)
{
    const bool result{pq::Pq::connect(_dburl)};
    if (result) {
        dburl = _dburl;
    } else {
        dburl.clear();
    }
    return result;
}

bool
Osm2Pgsql::getLastUpdateFromDb()
{
    if (isOpen()) {
        const std::string sql{str(format(R"sql(
        SELECT MAX(foo.ts) AS ts FROM(
          SELECT MAX(tags -> 'osm_timestamp') AS ts FROM %1%.planet_osm_point
          UNION
          SELECT MAX(tags -> 'osm_timestamp') AS ts FROM %1%.planet_osm_line
          UNION
          SELECT MAX(tags -> 'osm_timestamp') AS ts FROM %1%.planet_osm_polygon
          UNION
          SELECT MAX(tags -> 'osm_timestamp') AS ts FROM %1%.planet_osm_roads
        ) AS foo
      )sql") % schema)};
        try {
            const auto result{query(sql)};
            const auto row{result.at(0)};
            if (row.size() != 1) {
                return false;
            }
            auto timestamp{row[0].as<std::string>()};
            timestamp[10] = ' '; // Drop the 'T' in the middle
            timestamp.erase(19); // Drop the final 'Z'
            last_update = time_from_string(timestamp);
            return last_update != not_a_date_time;
        } catch (std::exception const &e) {
            log_error(_("Error getting last update from osm2pgsql DB: %1%"), e.what());
        }
        return false;
    } else {
        return false;
    }
}

const std::string &
Osm2Pgsql::getSchema() const
{
    return schema;
}

void
Osm2Pgsql::setSchema(const std::string &newSchema)
{
    schema = newSchema;
}

void
Osm2Pgsql::TagParser::parse(const std::map<std::string, std::string> &tags, const pqxx::nontransaction &worker, bool is_point)
{

    for (const auto &tag: tags) {
        if (z_index_map.find(tag) != z_index_map.cend()) {
            is_road = z_index_map.at(tag).second;
            z_order += z_index_map.at(tag).first;
        }
        if (!tag.second.empty()) {
            if (polygon_tags.find(tag.first) != polygon_tags.cend() || (tag.first == "area" && tag.second == "yes")) {
                is_polygon = true;
            }
            if (tags_array_literal.size() > 3) {
                tags_array_literal.append(",");
            }
            // Array literal needs backslash before double quotes
            const auto tag_value{worker.esc(tag.second)};
            const auto tag_name{worker.esc(tag.first)};
            const auto tag_name_safe{std::regex_replace(tag_name, tags_escape_re, R"raw(\\$1)raw")};
            const auto tag_value_safe{std::regex_replace(tag_value, tags_escape_re, R"raw(\\$1)raw")};
            tags_array_literal.append('"' + tag_name_safe + "\",\"" + tag_value_safe + '"');

            // z order
            if (tag.first == "layer") {
                try {
                    z_order += std::stoi(tag.second) * 10;
                } catch (const std::exception &ex) {
                    log_error(_("Error converting layer to integer: %1%"), ex.what());
                }
            }

            // Tags stored into columns
            if ((column_stored_tags.find(tag.first) != column_stored_tags.cend()) ||
                (is_point && column_points_stored_tags.find(tag.first) != column_points_stored_tags.cend())) {
                tag_field_names.append(separator + worker.quote_name(tag.first));
                const auto value{"E'" + worker.esc(tag.second) + "'"};
                tag_field_values.append(separator + value);
                tag_field_updates.append(separator + worker.quote_name(tag.first) + " = " + value);
            } else { // Tags added to "tags"
                tags_hstore_literal.append((tags_hstore_literal.size() > 3 ? ", \"" : "\"") + tag_name_safe + "\" => \"" +
                                           tag_value_safe + '"');
            }
        }
    }
    tags_array_literal.append("}'");
    tags_hstore_literal.append("'");
}

} // namespace osm2pgsql
