//
// Copyright (c) 2020, 2021, 2023 Humanitarian OpenStreetMap Team
//
// This file is part of Underpass.
//
//     Pq is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Pq is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Pq.  If not, see <https://www.gnu.org/licenses/>.
//

// This is generated by autoconf
#ifdef HAVE_CONFIG_H
#include "unconfig.h"
#endif

#include "data/pq.hh"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/locale.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "utils/log.hh"
using namespace logger;

namespace pq {

Pq::Pq() {};

Pq::Pq(const std::string &args) { connect(args); };

// Dump internal data to the terminal, used only for debugging
void
Pq::dump(void)
{
    log_debug("Database host: %1%", host);
    log_debug("Database name: %1%", dbname);
    log_debug("Database user: %1%", user);
    log_debug("Database password: %1%", passwd);
}

bool
Pq::isOpen() const
{
    return static_cast<bool>(sdb) && sdb->is_open();
}

bool
Pq::parseURL(const std::string &dburl)
{
    if (dburl.empty()) {
        log_error("No database connection string!");
        return false;
    }

    host.clear();
    port.clear();
    user.clear();
    passwd.clear();
    dbname.clear();

    std::size_t apos = dburl.find('@');
    if (apos != std::string::npos) {
        user = "user=";
        std::size_t cpos = dburl.find(':');
        if (cpos != std::string::npos) {
            user += dburl.substr(0, cpos);
            passwd = "password=";
            passwd += dburl.substr(cpos + 1, apos - cpos - 1);
        } else {
            user += dburl.substr(0, apos);
        }
    }

    std::vector<std::string> result;
    if (apos != std::string::npos) {
        boost::split(result, dburl.substr(apos + 1), boost::is_any_of("/"));
    } else {
        boost::split(result, dburl, boost::is_any_of("/"));
    }

    std::string host_tmp;
    std::string port_tmp;
    if (result.size() == 1) {
        if (apos == std::string::npos) {
            dbname = "dbname=" + result[0];
        } else {
            host_tmp = result[0];
        }
    } else if (result.size() == 2) {
        std::size_t colon_pos = result[0].find(':');
        if (colon_pos != std::string::npos) {
            host_tmp = result[0].substr(0, colon_pos);
            port_tmp = result[0].substr(colon_pos + 1);
        } else {
            host_tmp = result[0];
        }
        dbname = "dbname=" + result[1];
    }

    if (!host_tmp.empty()) {
        host = "host=" + host_tmp;
    }

    if (!port_tmp.empty()) {
        host += " port=" + port_tmp;
    }

    return true;
}


bool
Pq::connect(const std::string &dburl)
{
    std::string args;

    if (parseURL(dburl)) {
        args = host + " " + port + " " + dbname + " " + user + " " + passwd;
    } else {
        return false;
    }

    // log_debug(args);
    try {
        sdb = std::make_unique<pqxx::connection>(args);
        if (sdb->is_open()) {
            log_debug("Opened database connection to %1%", args);
            return true;
        } else {
            return false;
        }
    } catch (const std::exception &e) {
        log_error("Couldn't open database connection to %1% %2%", args,
                  e.what());
        return false;
    }
}

pqxx::result
Pq::query(const std::string &query)
{
    std::scoped_lock write_lock{pqxx_mutex};
    pqxx::work worker(*sdb);
    pqxx::result result = worker.exec(query);
    worker.commit();
    return result;
}

std::string
Pq::escapedString(std::string text)
{
    std::string newstr;
    int i = 0;
    while (i < text.size()) {
        if (text[i] == '\'') {
            newstr += "&apos;";
        } else if (text[i] == '\"') {
            newstr += "&quot;";
        } else if (text[i] == '\'') {
            newstr += "&quot;";
        } else if (text[i] == ')') {
            newstr += "&#41;";
        } else if (text[i] == '(') {
            newstr += "&#40;";
        } else if (text[i] == '\\') {
            // drop this character
        } else {
            newstr += text[i];
        }
        i++;
    }
    return sdb->esc(boost::locale::conv::to_utf<char>(newstr, "Latin1"));
}

} // namespace pq
