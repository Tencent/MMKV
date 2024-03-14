#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint mmkv.podspec' to validate before publishing.
#

Pod::Spec.new do |s|
  s.name             = 'mmkvflutter'
  s.version          = '1.3.4'
  s.summary          = 'MMKV is a cross-platform key-value storage framework developed by WeChat.'
  s.description      = <<-DESC
  The MMKV, for Flutter.
  MMKV is an efficient, complete, easy-to-use mobile key-value storage framework used in the WeChat application.
  It can be a replacement for NSUserDefaults & SharedPreferences.
                       DESC
  s.homepage     = "https://github.com/Tencent/MMKV"
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Tencent' => 'guoling@tencent.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.public_header_files = 'Classes/**/*.h'
  s.dependency 'Flutter'
  s.dependency 'MMKV', '>= 1.3.4'
  s.platform = :ios, '11.0'

# Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386',
#"GCC_PREPROCESSOR_DEFINITIONS" => "FORCE_POSIX",
  }

end

