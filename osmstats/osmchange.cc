//
// Copyright (c) 2020, 2001 Humanitarian OpenStreetMap Team
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
#include <codecvt>
#include <locale>
#include <pqxx/pqxx>
#ifdef LIBXML
# include <libxml++/libxml++.h>
#endif
#include <glibmm/convert.h>

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <ogrsf_frmts.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/progress.hpp>
// #include <boost/multi_index_container.hpp>
// #include <boost/multi_index/member.hpp>
// #include <boost/multi_index/ordered_index.hpp>
// using boost::multi_index_container;
// using namespace boost::multi_index;

#include "hotosm.hh"
#include "timer.hh"
#include "osmstats/osmchange.hh"
// #include "osmstats/geoutil.hh"
#include "data/osmobjects.hh"
using namespace osmobjects;

typedef boost::geometry::model::d2::point_xy<double> point_t;
typedef boost::geometry::model::polygon<point_t> polygon_t;
typedef boost::geometry::model::linestring<point_t> linestring_t;

#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1

namespace osmchange {

/// And OsmChange file contains the data of the actual change. It uses the same
/// syntax as an OSM data file plus the addition of one of the three actions.
/// Nodes, ways, and relations can be created, deleted, or modified.
///
/// <modify>
///     <node id="12345" version="7" timestamp="2020-10-30T20:40:38Z" uid="111111" user="foo" changeset="93310152" lat="50.9176152" lon="-1.3751891"/>
/// </modify>
/// <delete>
///     <node id="23456" version="7" timestamp="2020-10-30T20:40:38Z" uid="22222" user="foo" changeset="93310152" lat="50.9176152" lon="-1.3751891"/>
/// </delete>
/// <create>
///     <node id="34567" version="1" timestamp="2020-10-30T20:15:24Z" uid="3333333" user="bar" changeset="93309184" lat="45.4303763" lon="10.9837526"/>
///</create>

// Read a changeset file from disk or memory into internal storage
bool
OsmChangeFile::readChanges(const std::string &file)
{
    setlocale(LC_ALL, "");
    std::ifstream change;
    int size = 0;
    unsigned char *buffer;
    std::cout << "Reading OsmChange file " << file << std::endl;
    std::string suffix = boost::filesystem::extension(file);
    // It's a gzipped file, common for files downloaded from planet
    std::ifstream ifile(file, std::ios_base::in | std::ios_base::binary);
    if (suffix == ".gz") {  // it's a compressed file
        change.open(file,  std::ifstream::in |  std::ifstream::binary);
        try {
            boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
            inbuf.push(boost::iostreams::gzip_decompressor());
            inbuf.push(ifile);
            std::istream instream(&inbuf);
            // std::cout << instream.rdbuf();
            readXML(instream);
        } catch (std::exception& e) {
            std::cout << "ERROR opening " << file << std::endl;
            std::cout << e.what() << std::endl;
            // return false;
        }
    } else {                // it's a text file
        change.open(file, std::ifstream::in);
        readXML(change);
    }

    // FIXME: return a real value
    return false;
}

bool
OsmChangeFile::readXML(std::istream &xml)
{
    // std::cout << "OsmChangeFile::readXML(): " << xml.rdbuf();
    std::ofstream myfile;
#ifdef LIBXML
    // libxml calls on_element_start for each node, using a SAX parser,
    // and works well for large files.
    Timer timer;
    try {
        set_substitute_entities(true);
        timer.startTimer();
        parse_stream(xml);
        timer.endTimer("libxml++");
    }
    catch(const xmlpp::exception& ex) {
        std::cerr << "libxml++ exception: " << ex.what() << std::endl;
        std::cerr << xml.rdbuf() << std::endl;
        int return_code = EXIT_FAILURE;
    }
#else
    // Boost::parser_tree with RapidXML is faster, but builds a DOM tree
    // so loads the entire file into memory. Most replication files for
    // hourly or minutely changes are small, so this is better for that
    // case.
    boost::property_tree::ptree pt;
    Timer timer;
    timer.startTimer();
#ifdef USE_TMPFILE
    boost::property_tree::read_xml("tmp.xml", pt);
#else
    boost::property_tree::read_xml(xml, pt);
#endif

    timer.endTimer("parse_tree");

    if (pt.empty()) {
        std::cerr << "ERROR: XML data is empty!" << std::endl;
        // return false;
    }

//    boost::progress_display show_progress( 7000 );

    for (auto value: pt.get_child("osmChange")) {
        OsmChange change;
        action_t action;
        if (value.first == "modify") {
            change.action = modify;
        } else if (value.first == "create") {
            change.action = create;
        } else if (value.first == "remove") { // delete is a reserved word
            change.action = remove;
        }
        for (auto child: value.second) {
            // Process the tags. These don't exist for every element.
            // Both nodes and ways have tags
            std::cout << "CHILD: " << child.first << std::endl;
            for (auto tag: value.second) {
                if (tag.first == "tag") {
                    std::string key = tag.second.get("<xmlattr>.k", "");
                    std::string val = tag.second.get("<xmlattr>.v", "");
                    // static_cast<OsmNode *>(object)->addTag(key, val);
                } else if (tag.first == "nd") {
                    long ref = tag.second.get("<xmlattr>.ref", 0);
                    change.addRef(ref);
                }
           }
           // Only nodes have coordinates
           if (child.first == "node") {
               double lat = value.second.get("<xmlattr>.lat", 0.0);
               double lon = value.second.get("<xmlattr>.lon", 0.0);
               auto object = std::make_shared<OsmNode>();
               object->setLatitude(lat);
               object->setLongitude(lon);
               object->setAction(change.action);
               change.nodes.push_back(object);
           } else if (child.first == "way") {
               auto object = std::make_shared<OsmWay>();
               change.ways.push_back(object);
           } else if (child.first == "relation") {
               auto object = std::make_shared<OsmRelation>();
               change.relations.push_back(object);
           }
           
           // Process the attributes, which do exist in every element
           // change.id = value.second.get("<xmlattr>.id", 0);
           // change.version = value.second.get("<xmlattr>.version", 0);
           // change.timestamp = value.second.get("<xmlattr>.timestamp",
           //                   boost::posix_time::second_clock::local_time());
           // change.user = value.second.get("<xmlattr>.user", "");
           // change.uid = value.second.get("<xmlattr>.uid", 0);
           // changes.push_back(change);
           change.dump();
        }
//        ++show_progress;
    }
#endif
    // FIXME: return a real value
    return false;
}

#ifdef LIBXML
// Called by libxml++ for each element of the XML file
void
OsmChangeFile::on_start_element(const Glib::ustring& name,
                                const AttributeList& attributes)
{
    // If a change is in progress, apply to to that instance
    std::shared_ptr<OsmChange> change;
    // std::cout << "NAME: " << name << std::endl;
    // Top level element can be ignored
    if (name == "osmChange") {
        return;
    }
    // There are 3 change states to handle, each one contains possibly multiple
    // nodes and ways.
    if (name == "create") {
        change = std::make_shared<OsmChange>(osmobjects::create);
        changes.push_back(change);
        return;
    } else if (name == "modify") {
        change = std::make_shared<OsmChange>(osmobjects::modify);
        changes.push_back(change);
        return;
    } else if (name == "delete") {
        change = std::make_shared<OsmChange>(osmobjects::remove);
        changes.push_back(change);
        return;
    } else if (name == "node") {
        auto node = changes.back()->newNode();
        node->action = changes.back()->action;
    } else if (name == "tag") {
        // static_cast<std::shared_ptr<OsmWay>>(object)->tags.clear();
        // A tag element has only has 1 attribute, and numbers are stored as
        // strings
        // static_cast<OsmNode *>(object)->addTag(attributes[0].name, attributes[0].value);
    } else if (name == "way") {
        auto way = changes.back()->newWay();
        way->action = changes.back()->action;
    } else if (name == "relation") {
        auto relation = changes.back()->newRelation();
        relation->action = changes.back()->action;
    } else if (name == "member") {
        // It's a member of a relation
    } else if (name == "nd") {
        changes.back()->addRef(std::stol(attributes[0].value));
    }

    change = changes.back();

    // process the attributes
    std::string cache;
    for (const auto& attr_pair : attributes) {
        // Sometimes the data string is unicode
        // std::wcout << "\tPAIR: " << attr_pair.name << " = " << attr_pair.value << std::endl;
        // tags use a 'k' for the key, and 'v' for the value
        if (attr_pair.name == "ref") {
            //static_cast<OsmWay *>(object)->refs.push_back(std::stol(attr_pair.value));
        } else if (attr_pair.name == "k") {
            cache = attr_pair.value;
            continue;
        } else if (attr_pair.name == "v") {
            if (cache == "timestamp") {
                std::string tmp = attr_pair.value;
                tmp[10] = ' ';      // Drop the 'T' in the middle
                tmp.erase(19);      // Drop the final 'Z'
                change->setTimestamp(tmp);
            } else {
                change->addTag(cache, attr_pair.value);
                cache.clear();
            }
         } else if (attr_pair.name == "timestamp") {
            // Clean up the string to something boost can parse
            std::string tmp = attr_pair.value;
            tmp[10] = ' ';      // Drop the 'T' in the middle
            tmp.erase(19);      // Drop the final 'Z'
            change->setTimestamp(tmp);
        } else if (attr_pair.name == "id") {
            change->setChangeID(std::stol(attr_pair.value));
        } else if (attr_pair.name == "uid") {
            change->setUID(std::stol(attr_pair.value));
        } else if (attr_pair.name == "version") {
            change->setVersion(std::stod(attr_pair.value));
        } else if (attr_pair.name == "user") {
            change->setUser(attr_pair.value);
        } else if (attr_pair.name == "changeset") {
            change->setChangeID(std::stol(attr_pair.value));
        } else if (attr_pair.name == "lat") {
            change->setLatitude(std::stod(attr_pair.value));
        } else if (attr_pair.name == "lon") {
            change->setLongitude(std::stod(attr_pair.value));
        }
    }
}
#endif  // EOF LIBXML

void
OsmChange::dump(void)
{
    std::cout << "------------" << std::endl;
    std::cout << "Dumping OsmChange()" << std::endl;
    if (action == osmobjects::create) {
        std::cout << "\tAction: create" << std::endl;
    } else if(action == osmobjects::modify) {
        std::cout << "\tAction: modify" << std::endl;
    } else if(action == osmobjects::remove) {
        std::cout << "\tAction: delete" << std::endl;
    } else if(action == osmobjects::none) {
        std::cout << "\tAction: data element" << std::endl;
    }

    if (nodes.size() > 0) {
        std::cout << "\tDumping nodes:"  << std::endl;
        for (auto it = std::begin(nodes); it != std::end(nodes); ++it) {
            std::shared_ptr<OsmNode> node = *it;
            node->dump();
        }
    }
    if (ways.size() > 0) {
        std::cout << "\tDumping ways:" << std::endl;
        for (auto it = std::begin(ways); it != std::end(ways); ++it) {
            std::shared_ptr<OsmWay> way = *it;
            way->dump();
        }
    }
    if (relations.size() > 0) {
        for (auto it = std::begin(relations); it != std::end(relations); ++it) {
            // std::cout << "\tDumping relations: " << it->dump() << std::endl;
            // std::shared_ptr<OsmWay> rel = *it;
            // rel->dump();
        }
    }
}

void
OsmChangeFile::dump(void)
{
    std::cout << "Dumping OsmChangeFile()" << std::endl;
    std::cout << "There are " << changes.size() << " changes" << std::endl;
    for (auto it = std::begin(changes); it != std::end(changes); ++it) {
        OsmChange *change = it->get();
        change->dump();
    }
    if (userstats.size() > 0) {
        for (auto it = std::begin(userstats); it != std::end(userstats); ++it) {
            std::shared_ptr<ChangeStats> stats = it->second;
            stats->dump();
        }
    }
}

std::shared_ptr<std::map<long, std::shared_ptr<ChangeStats>>>
OsmChangeFile::collectStats(void)
{
    // struct user_id{};
    // struct change_id{};
    // typedef multi_index_container<ChangeStats, indexed_by<
    //     ordered_unique<
    //         tag<user_id>,  BOOST_MULTI_INDEX_MEMBER(ChangeStats,long,user_id)>,
    //     ordered_unique<
    //         tag<change_id>,  BOOST_MULTI_INDEX_MEMBER(ChangeStats,long,change_id)>>> stats;

    auto mstats = std::make_shared<std::map<long, std::shared_ptr<ChangeStats>>>();
    for (auto it = std::begin(changes); it != std::end(changes); ++it) {
        OsmChange *change = it->get();
        change->dump();
        if (change->action == create) {
            for (auto it = std::begin(change->nodes); it != std::end(change->nodes); ++it) {
                OsmNode *node = it->get();
                if (node->tags.size() > 0) {
                    std::cout << "New Node ID " << node->id << " has tags!" << std::endl;
                    auto cit = mstats->find(node->change_id);
                    if (cit != mstats->end()) {
                        cit->second->pois_added++;
                    } else {
                        auto ostats = std::make_shared<ChangeStats>();
                        ostats->pois_added++;
                        ostats->change_id = node->change_id;
                        ostats->user_id = node->uid;
                        mstats->insert(cit, std::pair(node->change_id, ostats));
                    }
                } else {
                    continue;
                }
            }
#if 0
            for (auto it = std::begin(change->ways); it != std::end(change->ways); ++it) {
                OsmWay *way = it->get();
                if (way->tags.size() == 0) {
                    std::cerr << "New Way ID " << way->id << " has no tags!" << std::endl;
                    if (way->isClosed() && way->numPoints() == 5) {
                        std::cerr << "WARNING: " << way->id << " might be a building!" << std::endl;
                    }
                    continue;
                }
                auto cit = mstats->find(way->change_id);
                if (cit != mstats->end()) {
                    if (way->tags.find("building") != way->tags.end()) {
                        cit->second->buildings_added++;
                    }
                    if (way->tags.find("highway") != way->tags.end()) {
                        cit->second->roads_km_added += way->getLength();
                        cit->second->roads_added++;
                    }
                    if (way->tags.find("waterway") != way->tags.end()) {
                        cit->second->waterways_km_added += way->getLength();
                        cit->second->waterways_added++;
                    }
                } else {
                    auto ostats = std::make_shared<ChangeStats>();
                    if (way->tags.find("building") != way->tags.end()) {
                        ostats->buildings_added++;
                    }
                    if (way->tags.find("highway") != way->tags.end()) {
                        ostats->roads_km_added += way->getLength();
                        ostats->roads_added++;
                    }
                    if (way->tags.find("waterway") != way->tags.end()) {
                        ostats->waterways_km_added += way->getLength();
                        ostats->waterways_added++;
                    }
                }
            }
#endif
#if 0
        } else if (change->action == modify) {   
            for (auto it = std::begin(change->nodes); it != std::end(change->nodes); ++it) {
                OsmNode *node = it->get();
                if (node->tags.size() > 0) {
                    std::cout << "Modified Node ID " << node->id << " has tags!" << std::endl;
                } else {
                    ostats.pois_modified++;
                }
            }
            for (auto it = std::begin(change->ways); it != std::end(change->ways); ++it) {
                OsmWay *way = it->get();
                if (way->tags.size() == 0) {
                    std::cerr << "Modified Way ID " << way->id << " has no tags!" << std::endl;
                    continue;
                }
                if (way->tags.find("building") != way->tags.end()) {
                    ostats.buildings_modified++;
                }
                if (way->tags.find("highway") != way->tags.end()) {
                    ostats.roads_km_modified += way->getLength();
                    ostats.roads_modified++;
                }
                if (way->tags.find("waterway") != way->tags.end()) {
                    ostats.waterways_km_modified += way->getLength();
                    ostats.waterways_modified++;
                }
            }
#endif
        }
    }

    return mstats;
}

/// Dump internal data to the terminal, only for debugging
void
ChangeStats::dump(void)
{
    std::cout << "Dumping ChangeStats for: \t " << change_id << std::endl;
    std::cout << "\tUser ID: \t\t " << user_id << std::endl;
    std::cout << "\tRoads Added (km): \t " << roads_km_added << std::endl;
    std::cout << "\tRoads Modified (km):\t " << roads_km_modified << std::endl;
    std::cout << "\tWaterways Added (km): \t " << waterways_km_added << std::endl;
    std::cout << "\tWaterways Modified (km): " << waterways_km_modified << std::endl;
    std::cout << "\tRoads Added: \t\t " << roads_added << std::endl;
    std::cout << "\tRoads Modified: \t " << roads_modified << std::endl;
    std::cout << "\tWaterways Added: \t " << waterways_added << std::endl;
    std::cout << "\tWaterways Modified: \t " << waterways_modified << std::endl;
    std::cout << "\tBuildings added: \t " << buildings_added << std::endl;
    std::cout << "\tBuildings Modified: \t " << buildings_modified << std::endl;
    std::cout << "\tPOIs added: \t\t " << pois_added << std::endl;
    std::cout << "\tPOIs Modified: \t\t " << pois_modified << std::endl;
};

} // EOF namespace osmchange

