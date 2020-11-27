# Add the function bellow to your app's Podfile, then call it, for example:
#
# 1. for a pure flutter app:
# fix_mmkv_plugin_name(File.dirname(File.realpath(__FILE__)))
#
# 2. for using flutter in existing app by a flutter module:
# flutter_application_path = 'path/to/your/flutter_module'
# fix_mmkv_plugin_name(flutter_application_path)


# to avoid conflict of the native lib name 'libMMKV.so' on iOS, we need to change the plugin name 'mmkv' to 'mmkvflutter'
def fix_mmkv_plugin_name(flutter_application_path)
  is_module = false
  plugin_deps_file = File.expand_path(File.join(flutter_application_path, '..', '.flutter-plugins-dependencies'))
  if not File.exists?(plugin_deps_file)
    is_module = true;
    plugin_deps_file = File.expand_path(File.join(flutter_application_path, '.flutter-plugins-dependencies'))
  end

  plugin_deps = JSON.parse(File.read(plugin_deps_file)).dig('plugins', 'ios') || []
  plugin_deps.each do |plugin|
    if plugin['name'] == 'mmkv' || plugin['name'] == 'mmkvflutter'
      require File.expand_path(File.join(plugin['path'], 'tool', 'mmkvpodhelper.rb'))
      mmkv_fix_plugin_name(flutter_application_path, is_module)
      return
    end
  end
  raise "Fail to find any mmkv plugin dependencies in #{plugin_deps_file}. If you're running pod install manually, make sure flutter pub get is executed first"
end

