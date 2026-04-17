CONTRIBUTING
============

ET: Legacy development is a collaborative effort done in an open, transparent and friendly manner.
Anyone is welcome to join our efforts!

## QUESTIONS

If you only have a question and don't want to read this whole document:

Do **NOT** file an issue to ask a question. You will get faster results by using the resources below:

* [etlegacy/#general](https://discordapp.com/channels/260750790203932672/260750790203932672) on Discord
* [#etlegacy](https://web.libera.chat/?channels=#etlegacy) on irc.libera.chat

See also our [FAQ](https://github.com/etlegacy/etlegacy/wiki/FAQ).

## BUG REPORTS

When reporting an issue, ensure to be as clear as possible, and include:

* description of the issue (current behavior, expected/desired behavior)
* minimal steps to reproduce the problem
* motivation and use case for changing the behavior (if relevant)
* version used, and behaviour on older versions (if relevant)

Do note that we can only fix issues happening in the engine or in our own mod. Issues specific to third party mods can't be fixed by us.

## FEATURE REQUESTS

The project has defined [scope](https://github.com/etlegacy/etlegacy/wiki/About) for its goals and ambition.

As a result, feature requests might not be accepted if they are considered out of scope.
In case of doubt, you might also want to discuss your idea with us before starting to implement it.

## CODING GUIDELINES

If you are interested to participate, ensure to read first our contribution guidelines:

* [How to commit your code](https://github.com/etlegacy/etlegacy/wiki/How-to-commit-Your-Code)
* [Coding conventions](https://github.com/etlegacy/etlegacy/wiki/Coding-Conventions)

## UPSTREAM AND FORKS

To pull the latest changes from the main ET: Legacy repository without merging yet:

```sh
pixi run upstream-fetch
# then: git log --oneline HEAD..FETCH_HEAD
# merge or rebase when ready: git merge FETCH_HEAD   # or your preferred workflow
```

Formatting and workflow checks used in CI can be run locally with `pixi install` and `pixi run -e validation check-changes`. The default comparison branch is read from `.upstream-remote-branch` when that file exists (for example `origin/main` on forks that use `main` as the default branch).

## COVERITY SCAN (OPTIONAL)

The scheduled **Coverity Scan** workflow (`.github/workflows/coverity-scan.yml`) does nothing unless you add repository **secrets** `COVERITY_SCAN_TOKEN` and `COVERITY_SCAN_EMAIL` from your [Coverity Scan](https://scan.coverity.com/) project. Without them, the workflow completes successfully and skips the build.

Optional **variable** `COVERITY_PROJECT` (repository **Actions → Variables**) should match your Coverity project slug (for example `timfox/sturmgeist`). If unset, the workflow defaults to `etlegacy/etlegacy` for compatibility with the upstream project.

## COMMUNICATION

Communication happens online at:

* [etlegacy/#etlegacy](https://discordapp.com/channels/260750790203932672/346956915814957067) on Discord
* [#etlegacy](https://web.libera.chat/?channels=#etlegacy) on irc.libera.chat
