Pod::Spec.new do |s|

  s.name         = "MMKVCore"
  s.version      = "1.3.5"
  s.summary      = "MMKVCore for MMKV. MMKV is a cross-platform key-value storage framework developed by WeChat."

  s.description  = <<-DESC
                      Don't use this library directly. Use MMKV instead.
                      MMKV is an efficient, complete, easy-to-use mobile key-value storage framework used in the WeChat application.
                      It can be a replacement for NSUserDefaults & SQLite.
                   DESC

  s.homepage     = "https://github.com/Tencent/MMKV"
  s.license      = { :type => "BSD 3-Clause", :file => "LICENSE.TXT"}
  s.author       = { "guoling" => "guoling@tencent.com" }

  s.ios.deployment_target = "12.0"
  s.osx.deployment_target = "10.13"
  s.tvos.deployment_target = "13.0"
  s.watchos.deployment_target = "4.0"
  s.visionos.deployment_target = "1.0"

  s.source       = { :git => "https://github.com/Tencent/MMKV.git", :tag => "v#{s.version}" }
  s.source_files = "Core", "Core/*.{h,cpp,hpp}", "Core/aes/*", "Core/aes/openssl/*", "Core/crc32/*.h"
  s.public_header_files = "Core/MMBuffer.h", "Core/MMKV.h", "Core/MMKVLog.h", "Core/MMKVPredef.h", "Core/PBUtility.h", "Core/ScopedLock.hpp", "Core/ThreadLock.h", "Core/aes/openssl/openssl_md5.h", "Core/aes/openssl/openssl_opensslconf.h"
  s.compiler_flags = '-x objective-c++'

  s.requires_arc = ['Core/MemoryFile.cpp', 'Core/ThreadLock.cpp', 'Core/InterProcessLock.cpp', 'Core/MMKVLog.cpp', 'Core/PBUtility.cpp', 'Core/MemoryFile_OSX.cpp', 'aes/openssl/openssl_cfb128.cpp', 'aes/openssl/openssl_aes_core.cpp', 'aes/openssl/openssl_md5_one.cpp', 'aes/openssl/openssl_md5_dgst.cpp', 'aes/AESCrypt.cpp']

  s.framework    = "CoreFoundation"
  s.ios.frameworks = "UIKit"
  s.libraries    = "z", "c++"
  s.pod_target_xcconfig = {
    "CLANG_CXX_LANGUAGE_STANDARD" => "gnu++17",
    "CLANG_CXX_LIBRARY" => "libc++",
    "CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF" => "NO",
  }

end

