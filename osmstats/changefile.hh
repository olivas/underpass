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

#ifndef __CHANGESET_HH__
#define __CHANGESET_HH__

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
#include <libxml++/libxml++.h>

#include <boost/date_time.hpp>
#include "boost/date_time/posix_time/posix_time.hpp"
using namespace boost::posix_time;
using namespace boost::gregorian;

#include "hotosm.hh"

namespace changefile {

class ChangeFile : public osmium::handler::Handler
{
public:
    //ChangeFile(void) override;
    //~ChangeFile(void) override;
    // Constructor. New data will be added to the given buffer.
    explicit ChangeFile(osmium::memory::Buffer& buffer) :
        m_buffer(buffer) {
    }

    /// The node handler is called for each node in the input data.
    void node(const osmium::Node& node) {
        // Open a new scope, because the NodeBuilder we are creating has to
        // be destructed, before we can call commit() below.
        {
            // To create a node, we need a NodeBuilder object. It will create
            // the node in the given buffer.
            osmium::builder::NodeBuilder builder{m_buffer};

            // Copy common object attributes over to the new node.
            copy_attributes(builder, node);

            // Copy the location over to the new node.
            builder.set_location(node.location());

            // Copy (changed) tags.
            copy_tags(builder, node.tags());
        }

        // Once the object is written to the buffer completely, we have to call
        // commit().
        m_buffer.commit();
    }

    /// The way handler is called for each way in the input data.
    void way(const osmium::Way& way) {
        {
            osmium::builder::WayBuilder builder{m_buffer};
            copy_attributes(builder, way);
            copy_tags(builder, way.tags());

            // Copy the node list over to the new way.
            builder.add_item(way.nodes());
        }
        m_buffer.commit();
    }

    /// The relation handler is called for each relation in the input data.
    void relation(const osmium::Relation& relation) {
        {
            osmium::builder::RelationBuilder builder{m_buffer};
            copy_attributes(builder, relation);
            copy_tags(builder, relation.tags());

            // Copy the relation member list over to the new way.
            builder.add_item(relation.members());
        }
        m_buffer.commit();
    }

private:
    /// Copy attributes common to all OSM objects (nodes, ways, and relations).
    template <typename T>
    void copy_attributes(T& builder, const osmium::OSMObject& object) {
        // The setter functions on the builder object all return the same
        // builder object so they can be chained.
        if (object.version() == 1) {
            std::cout << "FIXME: " << object.version() << std::endl;
        }
        builder.set_id(object.id())
            .set_version(object.version())
            .set_changefile(object.changefile())
            .set_timestamp(object.timestamp())
            .set_uid(object.uid())
            .set_user(object.user());
    }

    // Copy all tags with two changes:
    // * Do not copy "created_by" tags
    // * Change "landuse=forest" into "natural=wood"
    static void copy_tags(osmium::builder::Builder& parent, const osmium::TagList& tags) {

        // The TagListBuilder is used to create a list of tags. The parameter
        // to create it is a reference to the builder of the object that
        // should have those tags.
        osmium::builder::TagListBuilder builder{parent};

        // Iterate over all tags and build new tags using the new builder
        // based on the old ones.
        for (const auto& tag : tags) {
            if (!std::strcmp(tag.key(), "created_by")) {
                // ignore
            } else if (!std::strcmp(tag.key(), "landuse") && !std::strcmp(tag.value(), "forest")) {
                // add_tag() can be called with key and value C strings
                builder.add_tag("natural", "wood");
            } else {
                // add_tag() can also be called with an osmium::Tag
                builder.add_tag(tag);
            }
        }
    }    
    pqxx::connection *db;
    pqxx::work *worker;
    apidb::QueryStats osmdb;
    osmium::memory::Buffer& m_buffer;    
};
    
}       // EOF changefile

#endif  // EOF __CHANGEFILE_HH__
