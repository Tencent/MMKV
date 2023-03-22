#
# Tencent is pleased to support the open source community by making
# MMKV available.
#
# Copyright (C) 2020 THL A29 Limited, a Tencent company.
# All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use
# this file except in compliance with the License. You may obtain a copy of
# the License at
#
#       https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# from ruby 3.2  File.exists is broken, we need compat function
def mmkv_file_exists(file)
  is_exist = false
  if File.methods.include?(:exists?)
    is_exist = File.exists? file
  else
    is_exist = File.exist? file
  end
  return is_exist
end

# to avoid conflict of the native lib name 'libMMKV.so' on iOS, we need to change the plugin name 'mmkv' to 'mmkvflutter'
def fix_mmkv_plugin_name_inside_dependencies(plugin_deps_file)
  plugin_deps_file = File.expand_path(plugin_deps_file)
  unless mmkv_file_exists(plugin_deps_file)
    raise "#{plugin_deps_file} must exist. If you're running pod install manually, make sure flutter pub get is executed first.(mmkvpodhelper.rb)"
  end

  plugin_deps = JSON.parse(File.read(plugin_deps_file))
  (plugin_deps.dig('plugins', 'ios') || []).each do |plugin|
    if plugin['name'] == 'mmkv'
      plugin['name'] = 'mmkvflutter'

      json = plugin_deps.to_json
      File.write(plugin_deps_file, json)
      return
    end
  end
end

# to avoid conflict of the native lib name 'libMMKV.so' on iOS, we need to change the plugin name 'mmkv' to 'mmkvflutter'
def fix_mmkv_plugin_name_inside_registrant(plugin_registrant_path, is_module)
  if is_module
    plugin_registrant_file = File.expand_path(File.join(plugin_registrant_path, 'FlutterPluginRegistrant.podspec'))
    if mmkv_file_exists(plugin_registrant_file)
      registrant = File.read(plugin_registrant_file)
      if registrant.sub!("dependency 'mmkv'", "dependency 'mmkvflutter'")
        File.write(plugin_registrant_file, registrant)
      end
    end
  end

  plugin_registrant_source = is_module ? File.expand_path(File.join(plugin_registrant_path, 'Classes', 'GeneratedPluginRegistrant.m'))
    : File.expand_path(File.join(plugin_registrant_path, 'GeneratedPluginRegistrant.m'))
  if mmkv_file_exists(plugin_registrant_source)
    registrant_source = File.read(plugin_registrant_source)
    if registrant_source.gsub!(/\bmmkv\b/, 'mmkvflutter')
      File.write(plugin_registrant_source, registrant_source)
    end
  end
end

def mmkv_fix_plugin_name(flutter_application_path, is_module)
  if is_module
    flutter_dependencies_path = File.join(flutter_application_path, '.flutter-plugins-dependencies')
    fix_mmkv_plugin_name_inside_dependencies(flutter_dependencies_path)

    flutter_registrant_path = File.join(flutter_application_path, '.ios', 'Flutter', 'FlutterPluginRegistrant')
    fix_mmkv_plugin_name_inside_registrant(flutter_registrant_path, is_module)
  else
    flutter_dependencies_path = File.join(flutter_application_path, '..', '.flutter-plugins-dependencies')
    fix_mmkv_plugin_name_inside_dependencies(flutter_dependencies_path)

    flutter_registrant_path = File.join(flutter_application_path, 'Runner')
    fix_mmkv_plugin_name_inside_registrant(flutter_registrant_path, is_module)
  end
end

def load_mmkv_plugin_deps(flutter_application_path)
  func, flutter_dependencies_path, is_module = (defined? flutter_parse_plugins_file) ? 
    [method(:flutter_parse_plugins_file), File.join(flutter_application_path, '..', '.flutter-plugins-dependencies'), false]
  : (defined? flutter_parse_dependencies_file_for_ios_plugin) ? 
    [method(:flutter_parse_dependencies_file_for_ios_plugin), File.join(flutter_application_path, '.flutter-plugins-dependencies'), true]
  : [nil, nil, false]
  unless func
    raise "must load/require flutter's podhelper.rb before calling #{__method__}"
  end
  plugin_deps = func.call(flutter_dependencies_path)
  return plugin_deps
end