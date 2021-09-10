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

/// \brief      This file is for supporting the changetset file format, not
/// to be confused with the OSM Change format. This format is the one used
/// for files from planet, and also supports replication updates. There appear
/// to be no parsers for this format, so this was created to fill that gap.
/// The files are compressed in gzip format, so uncompressing has to be done
/// internally before parsing the XML
///

#ifndef __CHANGESET_HH__
#define __CHANGESET_HH__

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include <array>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
// #include <pqxx/pqxx>
#ifdef LIBXML
#include <libxml++/libxml++.h>
#endif

#include "boost/date_time/posix_time/posix_time.hpp"
#include <boost/date_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

#include "data/osmobjects.hh"
#include "osmstats/osmstats.hh"
#include "timer.hh"

// Forward declaration
namespace osmstats {
class RawCountry;
};

// Forward declaration
namespace geoutil {
class GeoUtil;
};

/// \namespace changeset
namespace changeset {

/// Check a character in a string if it'S a control character
extern bool
IsControl(int i);

extern std::string
fixString(std::string text);

/// \file changeset.hh
/// \brief The file is used for processing changeset files
///
/// The Changeset file contains the raw data on just the change,
/// and doesn't contain any data of the change except the comment
/// and hashtags used when the change was uploaded to OSM.

/// \class ChangeSet
/// \brief Data structure for a single changeset
///
/// This stores the hashtags and comments use for a change when it is
/// uploaded to OSM
class ChangeSet
{
  public:
    ChangeSet(void){/* FIXME */};
#ifdef LIBXML
    ChangeSet(const std::deque<xmlpp::SaxParser::Attribute> attrs);
#endif

    /// Dump internal data to the terminal, used only for debugging
    void dump(void);

    /// Add a hashtag to internal storage
    void addHashtags(std::string text) { hashtags.push_back(fixString(text)); };

    /// Add the comment field, which is often used for hashtags
    void addComment(std::string text) { comment = fixString(text); };

    /// Add the editor field
    void addEditor(std::string text) { editor = fixString(text); };

    // osmstats::RawCountry country;
    int countryid = -1;
    // protected so testcases can access private data
    // protected:
    // These fields come from the changeset replication file
    long id = 0;       ///< The changeset id
    ptime created_at;  ///< Creation starting timestamp for this changeset
    ptime closed_at;   ///< Creation ending timestamp for this changeset
    bool open = false; ///< Whether this changeset is still in progress
    std::string user;  ///< The OSM user name making this change
    long uid = 0;      ///< The OSM user ID making this change
    double min_lat =
        0.0; ///< The minimum latitude for the bounding box of this change
    double min_lon =
        0.0; ///< The minimum longitude for the bounding box of this change
    double max_lat =
        0.0; ///< The maximum latitude for the bounding box of this change
    double max_lon =
        0.0; ///< The maximum longitude for the bounding box of this change
    int num_changes = 0;    ///< The number of changes in this changeset, which
                            ///< apears to be unused
    int comments_count = 0; ///< The number of comments in this changeset, which
                            ///< apears to be unused
    std::vector<std::string>
        hashtags;        ///< Internal aray of hashtags in this changeset
    std::string comment; ///< The comment for this changeset
    std::string editor;  ///< The OSM editor the end user used
    std::string source;  ///< The imagery source
    std::map<std::string, std::string> tags;
    polygon_t bbox;
    bool priority;
};

/// \class ChangeSetFile
/// \brief This file reads a changeset file
///
/// This class reads a changeset file, as obtained from the OSM
/// planet server. This format is not supported by other tools,
/// so we add it there. As changeset file contains multiple changes,
// this contains data for the entire file.
#ifdef LIBXML
class ChangeSetFile : public xmlpp::SaxParser
#else
class ChangeSetFile
#endif
{
  public:
    ChangeSetFile(void){};

    void areaFilter(const multipolygon_t &poly);

    /// Read a changeset file from disk or memory into internal storage
    bool readChanges(const std::string &file);

    /// Read a changeset file from disk or memory into internal storage
    bool readChanges(const std::vector<unsigned char> &buffer);

    /// Import a changeset file from disk and initialize the database
    bool importChanges(const std::string &file);

#ifdef LIBXML
    /// Called by libxml++ for the start of each element in the XML file
    void on_start_element(const Glib::ustring &name,
                          const AttributeList &properties) override;
    /// Called by libxml++ for the end of each element in the XML file
    void on_end_element(const Glib::ustring &name) override;
#endif

    /// Read an istream of the data and parse the XML
    bool readXML(std::istream &xml);

    // /// Setup the boundary data used to determine the country
    // bool setupBoundaries(std::shared_ptr<GeoUtil> &geou) {
    //     // boundaries = geou;
    //     return false;
    // };

    /// Get one set of change data from the parsed XML data
    // ChangeSet& operator[](int index) { return changes[index]; };

    /// Dump the data of this class to the terminal. This should only
    /// be used for debugging.
    void dump(void);
    // protected:
    //     bool store;
    std::string filename; ///< The filename of this changeset for disk files
    std::list<std::shared_ptr<ChangeSet>>
        changes; ///< Storage of all the changes in this data
    // std::shared_ptr<GeoUtil> boundaries; ///< A pointer to the geoboundary
    // data
};
} // namespace changeset

#endif // EOF __CHANGESET_HH__
