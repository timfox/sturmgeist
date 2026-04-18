# Patches for upstream repositories

## `etlegacy-libs` (git submodule `libs/`)

**`0001-etlegacy-libs-p0-curl-openssl-wolfssl.patch`** bumps bundled **curl 8.12.1**, **OpenSSL 3.2.6**, and **wolfSSL 5.7.6-stable** in `CMakeLists.txt` (and removes unused wolfSSL patch wiring).

Apply inside a clone of [etlegacy/etlegacy-libs](https://github.com/etlegacy/etlegacy-libs) checked out at the same commit as this repo’s `libs` submodule, then open a PR upstream:

```sh
cd etlegacy-libs
git am /path/to/sturmgeist/misc/patches/0001-etlegacy-libs-p0-curl-openssl-wolfssl.patch
# or: git apply --check … then git apply
```

After the PR is merged, update the `libs` submodule pointer in this repo to the new `etlegacy-libs` commit.
