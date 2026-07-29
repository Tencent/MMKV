# mmkv\_ios

The iOS implementation detail of [`MMKV`][1].

## Usage

Never use it alone. It's NOT a complete set of MMKV functionality. It just provides the FFI implementation needed by MMKV.

This package is [endorsed][2], which means you can simply use `mmkv`
normally. This package will be automatically included in your app when you do,
so you do not need to add it to your `pubspec.yaml`.

However, if you `import` this package to use any of its APIs directly, you
should add it to your `pubspec.yaml` as usual.

## Local MMKV checkout

To run a Flutter app against MMKV sources from a local checkout instead of the
tagged Swift package, set `MMKV_LOCAL_PACKAGE_PATH` to the checkout root:

```bash
MMKV_LOCAL_PACKAGE_PATH=/path/to/MMKV flutter run
```

[1]: https://pub.dev/packages/mmkv
[2]: https://flutter.dev/docs/development/packages-and-plugins/developing-packages#endorsed-federated-plugin
