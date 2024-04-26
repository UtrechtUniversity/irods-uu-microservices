/**
 * \file
 * \brief     iRODS microservice to list all files and subdirectories in a directory
 * \author    Lazlo Westerhof
 * \copyright Copyright (c) 2024, Utrecht University
 *
 * This file is part of irods-uu-microservices.
 *
 * irods-uu-microservices is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * irods-uu-microservices is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with irods-uu-microservices.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "irods_includes.hh"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <string>
#include <vector>

namespace fs = boost::filesystem;
namespace pt = boost::property_tree;

int msiDirList(msParam_t* _path, msParam_t* _list, ruleExecInfo_t* _rei)
{
    // Convert parameter values to C strings.
    char* path_str = parseMspForStr(_path);
    if (!path_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    char* list_str = parseMspForStr(_list);
    if (!list_str) {
        return SYS_INVALID_INPUT_PARAM;
    }

    // Check that user is rodsadmin.
    if (_rei->uoic->authInfo.authFlag < LOCAL_PRIV_USER_AUTH) {
        return SYS_USER_NO_PERMISSION;
    }

    fs::path path_bp(path_str);
    try {
        // Does path exist?
        if (fs::exists(path_bp)) {
            // Is path a directory?
            if (fs::is_directory(path_bp)) {
                pt::ptree jsonResult;
                fs::directory_iterator endIter;

                // Iterate through directory.
                for (fs::directory_iterator iter(path_bp); iter != endIter; ++iter) {
                    const fs::directory_entry entry = *iter;
                    pt::ptree entryJson;

                    entryJson.put("name", entry.path().filename().string());

                    if (fs::is_regular_file(entry.status())) {
                        entryJson.put("type", "file");
                    }
                    else if (fs::is_directory(entry.status())) {
                        entryJson.put("type", "directory");
                    }
                    else if (fs::is_symlink(entry.symlink_status())) {
                        entryJson.put("type", "symlink");
                    }

                    if (fs::is_regular_file(entry.status())) {
                        entryJson.put("filesize", fs::file_size(entry.path()));
                    }

                    if (fs::is_directory(entry.status())) {
                        int numEntries = std::distance(fs::directory_iterator(entry.path()), fs::directory_iterator{});
                        entryJson.put("subdirectory_entries", numEntries);
                    }

                    std::time_t modifiedTime = fs::last_write_time(entry.path());
                    entryJson.put("modified_timestamp", std::asctime(std::localtime(&modifiedTime)));

                    jsonResult.push_back(std::make_pair("", entryJson));
                }

                std::stringstream ss;
                pt::write_json(ss, jsonResult);
                fillStrInMsParam(_list, ss.str().c_str());
            }
            else {
                rodsLog(LOG_ERROR, "msi_dir_list: path <%s> is not a directory", path_str);
                return SYS_INVALID_FILE_PATH;
            }
        }
        else {
            rodsLog(LOG_ERROR, "msi_dir_list: path <%s> does not exist", path_str);
            return SYS_INVALID_FILE_PATH;
        }
    }
    catch (const fs::filesystem_error& error) {
        rodsLog(LOG_ERROR, "msi_dir_list: filesystem error for <%s> - %s", path_str, error.what());
        return SYS_INVALID_FILE_PATH;
    }

    _rei->status = 0;

    return _rei->status;
}

extern "C" irods::ms_table_entry* plugin_factory()
{
    irods::ms_table_entry* msvc = new irods::ms_table_entry(2);

    msvc->add_operation<msParam_t*, msParam_t*, ruleExecInfo_t*>(
        "msiDirList", std::function<int(msParam_t*, msParam_t*, ruleExecInfo_t*)>(msiDirList));

    return msvc;
}
