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

/// \file statsconfig.cc
/// \brief Statistis configuration parser and object builder
///
/// Read configuration files for statistics and produce a data structure

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
# include "unconfig.h"
#endif

#include "yaml2.hh"
#include "statsconfig.hh"
#include "galaxy/osmchange.hh"

/// \namespace statsconfig
namespace statsconfig {

    StatsConfig::StatsConfig(std::string name) {
        this->name = name;
    }
    StatsConfig::StatsConfig (
        std::string name,
        std::map<std::string, std::vector<std::string>> way,
        std::map<std::string, std::vector<std::string>> node
    ) 
    {
        this->name = name;
        this->way = way;
        this->node = node;
    };

    int StatsConfigFile::read_yaml(std::string filename, std::vector<StatsConfig> &statsconfig) {
        yaml2::Yaml2 yaml;
        yaml.read(filename);
        for (auto it = std::begin(yaml.root.children); it != std::end(yaml.root.children); ++it) {
            std::map<std::string, std::vector<std::string>> way_tags;
            std::map<std::string, std::vector<std::string>> node_tags;
            for (auto type_it = std::begin(it->children); type_it != std::end(it->children); ++type_it) {
                for (auto value_it = std::begin(type_it->children); value_it != std::end(type_it->children); ++value_it) {
                    for (auto tag_it = std::begin(value_it->children); tag_it != std::end(value_it->children); ++tag_it) {
                        if (type_it->value == "way") {
                            way_tags[value_it->value].push_back(tag_it->value);
                        } else if (type_it->value == "node") {
                            node_tags[value_it->value].push_back(tag_it->value);
                        }
                    }
                }            
            }
            statsconfig.push_back(StatsConfig(it->value, way_tags, node_tags));
        }
        return 1;
    }

    std::string StatsConfigSearch::category(std::string tag, std::string value, std::map<std::string, std::vector<std::string>> tags) {
        for (auto tag_it = std::begin(tags); tag_it != std::end(tags); ++tag_it) {
            if (tag == tag_it->first) {
                for (auto value_it = std::begin(tag_it->second); value_it != std::end(tag_it->second); ++value_it) {
                    if (*value_it == "*" || value == *value_it) {
                        return *value_it;
                    }
                }
            }
        }
        return "";
    };
    std::string StatsConfigSearch::tag_value(std::string tag, std::string value, osmchange::osmtype_t type, std::vector<StatsConfig> &statsconfig) {
        std::string category = "";
        for (int i = 0; i < statsconfig.size(); ++i) {
            if (type == osmchange::way) {
                category = StatsConfigSearch::category(tag, value, statsconfig[i].way);
                if (category != "") {
                    return statsconfig[i].name;
                }
            } else if (type == osmchange::node) {
                category = StatsConfigSearch::category(tag, value, statsconfig[i].node);
                if (category != "") {
                    return statsconfig[i].name;
                }
            }
        }

        return category;
    }



} // EOF statsconfig namespace

// Local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
