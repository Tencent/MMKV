Pod::Spec.new do |s|

  s.name         = "MMKVWatchExtension"
  s.version      = "1.3.3"
  s.summary      = "MMKV is a cross-platform key-value storage framework developed by WeChat."
  s.module_name  = "MMKVWatchExtension"

  s.description  = <<-DESC
                      The MMKV for WatchOS App Extensions.
                      MMKV is an efficient, complete, easy-to-use mobile key-value storage framework used in the WeChat application.
                      It can be a replacement for NSUserDefaults & SQLite.
                   DESC

  s.homepage     = "https://github.com/Tencent/MMKV"
  s.license      = { :type => "BSD 3-Clause", :file => "LICENSE.TXT"}
  s.author       = { "guoling" => "guoling@tencent.com" }

  s.watchos.deployment_target = "4.0"

  s.source       = { :git => "https://github.com/Tencent/MMKV.git", :tag => "v#{s.version}" }
  s.source_files =  "iOS/MMKV/MMKV", "iOS/MMKV/MMKV/*.{h,mm,hpp}"
  s.public_header_files = "iOS/MMKV/MMKV/MMKV.h", "iOS/MMKV/MMKV/MMKVHandler.h"

  s.framework    = "CoreFoundation"
  s.libraries    = "z", "c++"
  s.requires_arc = false
  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "gnu++17",
    "CLANG_CXX_LIBRARY" => "libc++",
    "CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF" => "NO",
    "GCC_PREPROCESSOR_DEFINITIONS" => "MMKV_IOS_EXTENSION",
  }

  s.dependency 'MMKVCore', '~> 1.3.3'

end

