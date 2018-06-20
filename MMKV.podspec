Pod::Spec.new do |s|

  s.name         = "MMKV"
  s.version      = "1.0.8"
  s.summary      = "MMKV is a cross-platform key-value storage framework developed by WeChat."

  s.description  = <<-DESC
                      The MMKV, for Objective-C.
                      MMKV is an efficient, complete, easy-to-use mobile key-value storage framework used in the WeChat application.
                      It can be a replacement for NSUserDefaults & SQLite.
                   DESC

  s.homepage     = "http://git.code.oa.com/wechat-team/mmkv"
  s.license      = { :type => "BSD", :file => "LICENSE"}
  s.author       = { "guoling" => "guoling@tencent.com" }

  s.platform     = :ios, "8.0"
  s.ios.deployment_target = "8.0"

  s.source       = { :git => "http://git.code.oa.com/wechat-team/mmkv/mmkv.git", :tag => "v#{s.version}" }
  s.source_files =  "iOS/MMKV/MMKV", "iOS/MMKV/MMKV/*.{h,mm,hpp}"
  s.public_header_files = "iOS/MMKV/MMKV/MMKV.h"

  s.framework    = "CoreFoundation"
  s.ios.frameworks = "UIKit"
  s.libraries    = "z", "c++"
  s.requires_arc = true
  s.xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "gnu++14",
    "CLANG_CXX_LIBRARY" => "libc++",
    "CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF" => "NO",
  }

end
