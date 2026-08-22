/*
 * Tencent is pleased to support the open source community by making
 * MMKV available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef POSIX_INSTALLER_MODE
#include <MMKV/MMKVPredef.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

using namespace std;
namespace fs = filesystem;

string getenv_or(const string& name, const string& fallback)
{
    if (const char* value = getenv(name.c_str()))
        return value;

    return fallback;
}

void symlink_force(const fs::path & src, const fs::path & link){
    if (fs::exists(link) || fs::is_symlink(link))
        fs::remove(link);

    fs::create_symlink(
        src,
        link
    );
    cout << "Created symlink: "
    << link << " -> "
    << src << '\n';
}

int main()
{
    const fs::path destdir = getenv_or("DESTDIR", "/");

    const fs::path prefix = PREFIX;
    const fs::path libdir = LIBDIR;
    const fs::path includedir = INCLUDE;

    // Built library.
    const fs::path library_source = LIBRARY;

    // Source MMKV headers.
    const fs::path headers_source = INCLUDEDIR;

    // Final installation directories.
    const fs::path library_destination =
    destdir / prefix.relative_path() / libdir;

    const fs::path headers_destination =
    destdir / prefix.relative_path() / includedir / "MMKV";

    cout
    << "Installing mmkv " << MMKV_VERSION << '\n'
    << " from " << library_source << '\n'
    << " and headers from " << headers_source << '\n'
    << " to " << destdir << '\n'
    << '\n';

    cout
    << "PREFIX: " << prefix << '\n'
    << "LIBDIR: " << libdir << '\n'
    << "INCLUDE: " << includedir << '\n';

    try {
        // Create destination directories.
        fs::create_directories(library_destination);
        fs::create_directories(headers_destination);

        // create version
        string version = MMKV_VERSION;
        version[0] = '.';

        // Copy the library.
        const fs::path installed_library =
        library_destination / (library_source.filename().c_str() + version);

        fs::copy_file(
            library_source,
            installed_library,
            fs::copy_options::overwrite_existing
        );

        // Copy MMKV headers.
        fs::copy(
            headers_source,
            headers_destination,
            fs::copy_options::recursive |
            fs::copy_options::overwrite_existing
        );

        // Create libmmkv.so symlink.
        const fs::path link =
        library_destination / "libmmkv.so";

        auto second_dot = version.find('.', 1);
        auto third_dot = version.find('.', second_dot + 1);

        std::string major = version.substr(0, second_dot);
        std::string minor = version.substr(0, third_dot);
        const fs::path link_minor = link.c_str() + minor;
        const fs::path link_major = link.c_str() + major;


        cout << "Installed library: "
        << installed_library << '\n';

        cout << "Installed headers: "
        << headers_destination << '\n';

        symlink_force(
            installed_library.filename(),
                      link_minor
        );
        symlink_force(
            link_minor.filename(),
            link_major
        );
        symlink_force(
            link_major.filename(),
            link
        );
    }
    catch (const fs::filesystem_error& e) {
        cerr << "Installation failed: "
        << e.what() << '\n';
        return 1;
    }

    return 0;
}
#else
#include <MMKV/MMKV.h>
#endif
