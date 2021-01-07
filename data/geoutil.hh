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

#ifndef __GEOUTIL_HH__
#define __GEOUTIL_HH__

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

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/geometries.hpp>
#include <ogrsf_frmts.h>

#include "hotosm.hh"
#include "osmstats/osmstats.hh"
#include "timer.hh"

// Forward instantiate class
namespace osmstats {
class RawCountry;
};

/// \namespace geoutil
namespace geoutil {

/// \file geoutil.hh
/// \brief This file parses a data file containing country boundaries
///
/// This uses GDAL to read and parse a file in any supported format
/// and loads it into a data structure. This is used to determine which
///country a change was made in.

/// \typedef point_t
/// \brief Simplify access to boost geometry point
typedef boost::geometry::model::d2::point_xy<double> point_t;
/// \typedef ploygon_t
/// \brief Simplify access to boost geometry polygon
typedef boost::geometry::model::polygon<point_t> polygon_t;
/// \typedef linestring_t
/// \brief Simplify access to boost geometry linestring
typedef boost::geometry::model::linestring<point_t> linestring_t;

/// \class GeoCountry
/// \brief Data structure for country boundaries
///
/// This stores the data for a country boundary which is used to
/// determine the country that a change was made in.
class GeoCountry
{
public:
    GeoCountry(void) {};
    GeoCountry(std::string name, std::string iso_a2, std::string iso_a3,
               std::string region, std::string subregion,
               boost::geometry::model::polygon<point_t>);

    /// Set the name field
    void setName(const std::string &field) { name = field; };

    /// Set the alternate name field
    void setAltName(const std::string &field) { alt_name = field; };

    /// extract the tags from the string for all the other metadata
    int extractTags(const std::string &other);

    /// Add the boundary coordinates from a WKT string
    void addBoundary(const std::string &border) {
        boost::geometry::read_wkt(border, boundary);
    };

    /// Get the name for this country
    const std::string &getName(void) { return name; };
    /// Get the alternate name for this country, if there is one
    const std::string &getAltName(void) { return alt_name; };
    /// Get the Country ID for this country
    long getID(void) { return id; };
    const std::string &getAbbreviation(int width) {
        if (width == 2) {
            return iso_a2;
        } else {
            return iso_a3;
        }
    };

    /// Determine if a bounding box is contained within this country boundary
    bool inCountry(double max_lat, double max_lon, double min_lat, double min_lon);

    /// Dump internal data to the terminal, only for debugging
    void dump(void);
private:
    long id;                    ///< The Country ID
    std::string name;           // Default name
    std::string alt_name;       ///< The optional alternate name
    std::map<std::string, std::string> names; ///< International locale names

    std::string iso_a2;         ///< 2 letter ISO abbreviation
    std::string iso_a3;         ///< 3 letter ISO abbreviation
    // The region and subregion aren't currently used for anything
    std::string region;         ///< The region this country is in
    std::string subregion;      ///< The subregion this country is in
    polygon_t boundary;         ///< The boundary of this country
};


/// \class GeoUtil
/// \brief Read in the Country boundaries data file
///
/// This parses the data file in any GDAL supported format into a data
/// structure that can be used to determine which country a change was
/// made in.
class GeoUtil : public Timer
{
public:
    GeoUtil(void) {
        GDALAllRegister();
    };
    /// Read a file into internal storage so boost::geometry functions
    /// can be used to process simple geospatial calculations instead
    /// of using postgres. This data is is used to determine which
    /// country a change was made in, or filtering out part of the
    /// planet to reduce data size. Since this uses GDAL, any
    /// multi-polygon file of any supported format can be used.
    bool readFile(const std::string &filespec, bool multi);
    
    /// Connect to the geodata database
    bool connect(const std::string &dbserver, const std::string &database);

    /// Find the country the changeset was made in
    // osmstats::RawCountry findCountry(double lat, double lon);

    /// See if this changeset is in a focus area. We ignore changsets in
    /// areas like North America to reduce the amount of data needed
    /// for calulations. This boundary can always be modified to be larger.
    bool focusArea(double lat, double lon);

    /// See if the given location can be identified
    GeoCountry &inCountry(double max_lat, double max_lon, double min_lat, double min_lon);

    /// Export all the countries in the format used by OSM Stats, which
    /// doesn't use the geospatial data. This table needs to be regenerated
    /// using the same data file as used to geolocate which country a
    /// changeset is made it.
    std::vector<osmstats::RawCountry> exportCountries(void);

    /// Get parsed country data by name 
    GeoCountry &getCountry(const std::string &country) {
        // return countries[country];
    }
    /// Dump internal data storage for debugging purposes
    void dump(void);

private:
    std::vector<GeoCountry> countries; ///< all the countries boundaries
};
    
}       // EOF geoutil

#endif  // EOF __GEOUTIL_HH__
