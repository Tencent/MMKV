# Contributing to MMKV
Welcome to [report Issues](https://github.com/Tencent/MMKV/issues) or [pull requests](https://github.com/Tencent/MMKV/pulls). It's recommended to read the following Contributing Guide first before contributing. 

## Issues
We use issues to track public bugs and feature requests.

### Search Known Issues First
Please search the existing issues to see if any similar issue or feature request has already been filed. You should make sure your issue isn't redundant.

### Reporting New Issues
If you open an issue, the more information the better. Such as detailed description, screenshot or video of your problem, logcat or code blocks for your crash.

## Pull Requests
We strongly welcome your pull request to make MMKV better. 

### Branch Management
There are three main branches here:

1. `master` branch.
	1. It is the latest (pre-)release branch. We use `master` for tags, with version number `1.1.0`, `1.2.0`, `1.3.0`...
	2. **Don't submit any PR on `master` branch.**
2. `dev` branch. 
	1. It is our stable developing branch. After full testing, `dev` will be merged to `master` branch for the next release.
	2. **You are recommended to submit bugfix or feature PR on `dev` branch.**
3. `hotfix` branch. 
	1. It is the latest tag version for hot fix. If we accept your pull request, we may just tag with version number `1.1.1`, `1.2.3`.
	2. **Only submit urgent PR on `hotfix` branch for next specific release.**

Normal bugfix or feature request should be submitted to `dev` branch. After full testing, we will merge them to `master` branch for the next release. 

If you have some urgent bugfixes on a published version, but the `master` branch have already far away with the latest tag version, you can submit a PR on hotfix. And it will be cherry picked to `dev` branch if it is possible.

```
master
 ↑
dev        <--- hotfix PR
 ↑ 
feature/bugfix PR
```  

### Make Pull Requests
The code team will monitor all pull request, we run some code check and test on it. After all tests passed, we will accecpt this PR. But it won't merge to `master` branch at once, which have some delay.

Before submitting a pull request, please make sure the followings are done:

1. Fork the repo and create your branch from `master` or `hotfix`.
2. Update code or documentation if you have changed APIs.
3. Add the copyright notice to the top of any new files you've added.
4. Check your code lints and checkstyles.
5. Test and test again your code.
6. Now, you can submit your pull request on `dev` or `hotfix` branch.

## Code Style Guide
We choose the `LLVM code style` for MMKV project, with the specialization that using 4 space width for indent, and using tab for ObjC indentation. To make things simple, we have already defined our code style inside [clang-format](./.clang-format).  
You can just run `make format_code` on top directory to format all your changes before committing them.  

Additionly, check out [Code Style](./Android/MMKV/checkstyle.xml) for Java and Android.

## License
By contributing to MMKV, you agree that your contributions will be licensed
under its [BSD LICENSE](./LICENSE.txt)
