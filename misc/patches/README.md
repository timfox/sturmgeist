# Patches for the **`libs`** submodule

The **`libs/`** directory is a separate git repository (submodule in this tree). Patches here target that repo’s root **`CMakeLists.txt`** (bundled curl, OpenSSL, wolfSSL, and similar).

## P0 — curl, OpenSSL, wolfSSL

**`0001-libs-p0-curl-openssl-wolfssl.patch`** bumps bundled **curl 8.12.1**, **OpenSSL 3.2.6**, and **wolfSSL 5.7.6-stable** (and removes unused wolfSSL patch wiring).

Apply in a clone of the **same remote your `libs` submodule points at** (often [etlegacy/etlegacy-libs](https://github.com/etlegacy/etlegacy-libs)), checked out at the same commit as this repo’s **`libs`** submodule, then open a PR:

```sh
git clone <your-libs-remote-url> libs-work
cd libs-work
git checkout <same-commit-as-this-repo/libs>
git am /path/to/sturmgeist/misc/patches/0001-libs-p0-curl-openssl-wolfssl.patch
# or: git apply --check … then git apply
```

After the change is merged on the **`libs`** remote, in this repo run `cd libs && git fetch && git checkout <new-commit>` then commit the updated submodule pointer on your integration branch.
