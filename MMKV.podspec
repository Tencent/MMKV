Pod::Spec.new do |s|

  s.name         = "MMKV"
  s.version      = "1.0.23"
  s.summary      = "MMKV is a cross-platform key-value storage framework developed by WeChat."

  s.description  = <<-DESC
                      The MMKV, for Objective-C.
                      MMKV is an efficient, complete, easy-to-use mobile key-value storage framework used in the WeChat application.
                      It can be a replacement for NSUserDefaults & SQLite.
                   DESC

  s.homepage     = "https://github.com/Tencent/MMKV"
  s.license      = { :type => "BSD 3-Clause", :file => "LICENSE.TXT"}
  s.author       = { "guoling" => "guoling@tencent.com" }

  s.ios.deployment_target = "8.0"
  s.osx.deployment_target = "10.9"

  s.source       = { :git => "https://github.com/Tencent/MMKV.git", :tag => "v#{s.version}" }
  s.source_files =  "iOS/MMKV/MMKV", "iOS/MMKV/MMKV/*.{h,mm,hpp}", "iOS/MMKV/MMKV/aes/*", "iOS/MMKV/MMKV/aes/openssl/*"
  s.public_header_files = "iOS/MMKV/MMKV/MMKV.h", "iOS/MMKV/MMKV/MMKVHandler.h"

  s.framework    = "CoreFoundation"
  s.ios.frameworks = "UIKit"
  s.libraries    = "z", "c++"
  s.requires_arc = true
  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "gnu++17",
    "CLANG_CXX_LIBRARY" => "libc++",
    "CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF" => "NO",
  }

end
