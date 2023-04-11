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

#ifndef __OSMCHANGE_HH__
#define __OSMCHANGE_HH__

/// \file osmchange.hh
/// \brief This file parses a change file in the OsmChange format
///
/// This file parses an OsmChange formatted data file using an libxml++
/// SAX parser, which works better for large files, or boost parse trees
/// using a DOM parser, which is faster for small files.

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <list>

//#include <pqxx/pqxx>
#ifdef LIBXML
#include <libxml++/libxml++.h>
#endif
#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#define BOOST_BIND_GLOBAL_PLACEHOLDERS 1

#include "validate/validate.hh"
#include "osm/osmobjects.hh"
#include "osm/osmchange.hh"
#include <ogr_geometry.h>

/// \namespace osmchange
namespace osmchange {

/// \enum osmtype_t
/// The object types used by an OsmChange file
typedef enum { empty, node, way, relation, member } osmtype_t;

/// \class ChangeStats
/// \brief These are per user statistics
///
/// This stores the calculated data from a change for a user,
/// which later gets added to the database statistics.
class ChangeStats {
  public:
    long change_id = 0;   ///< The ID of this change
    long user_id = 0;     ///< The User ID
    std::string username; ///< The User Name
    ptime created_at;     ///< The starting timestamp
    ptime closed_at;      ///< The finished timestamp
    std::map<std::string, int> added; ///< Array of added features
    std::map<std::string, int> modified; ///< Array of modified features
    std::map<std::string, int> deleted; ///< Array of deleted features
    /// Dump internal data to the terminal, only for debugging
    void dump(void);
};

/// \class OsmChange
/// \brief This contains the data for a change
///
/// This contains all the data in a change. Redirection is used
/// so the object type has a generic API
class OsmChange {
  public:
    OsmChange(osmobjects::action_t act) { action = act; };

    ///< dump internal data, for debugging only
    void dump(void);

    // protected:
    /// Set the latitude of the current node
    void setLatitude(double lat)
    {
        if (type == node) {
            nodes.back()->setLatitude(lat);
        }
    };
    /// Set the longitude of the current node
    void setLongitude(double lon)
    {
        if (type == node) {
            nodes.back()->setLongitude(lon);
        }
    };
    /// Set the timestamp of the current node or way
    void setTimestamp(const std::string &val)
    {
        if (type == node) {
            nodes.back()->timestamp = time_from_string(val);
        }
        if (type == way) {
            ways.back()->timestamp = time_from_string(val);
        }
    };
    /// Set the version number of the current node or way
    void setVersion(double val)
    {
        if (type == node) {
            nodes.back()->version = val;
        }
        if (type == way) {
            ways.back()->version = val;
        }
    };
    /// Add a tag to the current node or way
    void addTag(const std::string &key, const std::string &value)
    {
        if (type == node) {
            nodes.back()->addTag(key, value);
        }
        if (type == way) {
            ways.back()->addTag(key, value);
        }
    };
    /// Add a node reference to the current way
    void addRef(long ref)
    {
        if (type == way) {
            ways.back()->addRef(ref);
        }
    };
    /// Add a member reference to the current relation
    void addMember(long ref, osmobjects::osmtype_t _type,
                   const std::string role)
    {
        if (type == relation && relations.size() > 0) {
            relations.back()->addMember(ref, _type, role);
        } else {
            log_debug("Could not add member to relation!");
        }
    };
    /// Set the User ID for the current node or way
    void setUID(long val)
    {
        if (type == node) {
            nodes.back()->uid = val;
        }
        if (type == way) {
            ways.back()->uid = val;
        }
    };
    /// Set the Change ID for the current node or way
    void setChangeID(long val)
    {
        if (type == node) {
            nodes.back()->id = val;
        }
        if (type == way) {
            ways.back()->id = val;
        }
    };
    /// Set the User name for the current node or way
    void setUser(const std::string &val)
    {
        if (type == node) {
            nodes.back()->user = val;
        }
        if (type == way) {
            ways.back()->user = val;
        }
    };
    /// Instantiate a new node
    std::shared_ptr<osmobjects::OsmNode> newNode(void)
    {
        auto tmp = std::make_shared<osmobjects::OsmNode>();
        type = node;
        nodes.push_back(tmp);
        return tmp;
    };
    /// Instantiate a new way
    std::shared_ptr<osmobjects::OsmWay> newWay(void)
    {
        std::shared_ptr<osmobjects::OsmWay> tmp =
            std::make_shared<osmobjects::OsmWay>();
        type = way;
        ways.push_back(tmp);
        return tmp;
    };
    /// Instantiate a new relation
    std::shared_ptr<osmobjects::OsmRelation> newRelation(void)
    {
        std::shared_ptr<osmobjects::OsmRelation> tmp =
            std::make_shared<osmobjects::OsmRelation>();
        type = relation;
        relations.push_back(tmp);
        return tmp;
    };

    ptime final_entry;    ///< The timestamp of the last change in the file
    osmobjects::action_t action = osmobjects::none; ///< The change action
    osmtype_t type;                                 ///< The OSM object type
    std::list<std::shared_ptr<osmobjects::OsmNode>> nodes; ///< The nodes in this change
    std::list<std::shared_ptr<osmobjects::OsmWay>> ways; ///< The ways in this change
    std::list<std::shared_ptr<osmobjects::OsmRelation>> relations; ///< The relations in this change
    std::shared_ptr<osmobjects::OsmObject> obj;
};

/// \class OsmChangeFile
/// \brief This class manages an OSM change file.
///
/// This class handles the entire OsmChange file. Currently libxml++
/// is used with a SAX parser, although optionally the boost parse tree
/// can be used as a DOM parser.
#ifdef LIBXML
class OsmChangeFile : public xmlpp::SaxParser
#else
class OsmChangeFile
#endif
{
  public:

    OsmChangeFile(void){};
    OsmChangeFile(const std::string &osc) {
        readChanges(osc);
    };

    /// Read a changeset file from disk or memory into internal storage
    bool readChanges(const std::string &osc);

    /// Delete any data not in the boundary polygon
    void areaFilter(const multipolygon_t &poly);

#ifdef LIBXML
    /// Called by libxml++ for each element of the XML file
    void on_start_element(const Glib::ustring &name,
                          const AttributeList &properties) override;
#endif

    /// Read an istream of the data and parse the XML
    bool readXML(std::istream &xml);

    std::map<long, std::shared_ptr<ChangeStats>> userstats; ///< User statistics for this file

    std::list<std::shared_ptr<OsmChange>> changes; ///< All the changes in this file
    std::map<long, point_t> nodecache;           ///< Cache nodes across multiple changesets

    /// Collect statistics for each user
    std::shared_ptr<std::map<long, std::shared_ptr<ChangeStats>>>
    collectStats(const multipolygon_t &poly);

    /// Validate multiple nodes
    std::shared_ptr<std::vector<std::shared_ptr<ValidateStatus>>>
    validateNodes(const multipolygon_t &poly, std::shared_ptr<Validate> &plugin);

    /// Validate multi ways
    std::shared_ptr<std::vector<std::shared_ptr<ValidateStatus>>>
    validateWays(const multipolygon_t &poly, std::shared_ptr<Validate> &plugin);

    /// Scan tags for the proper values
    std::shared_ptr<std::vector<std::string>>
    scanTags(std::map<std::string, std::string> tags, osmchange::osmtype_t type);

//    std::map<long, bool> priority;
    /// dump internal data, for debugging only
    void dump(void);

};

} // namespace osmchange

#endif // EOF __OSMCHANGE_HH__

// local Variables:
// mode: C++
// indent-tabs-mode: nil
// End:
