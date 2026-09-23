# Versioning

We keep track of 3 version numbers: The release of Nintendont, Nintendont itself, and the API of the remote protocol.

The releases use Sequential Versioning, meaning that each release increases the version number by one.  

Nintendont's version number consistes of MAJOR.MINOR-multiworld, where:
- MAJOR is kept in sync with upstream Nintendont.
- MINOR is increased by one on every new release.
To find out the Nintendont version, consult the Homebrew Channel metadata.

The Remote Protocol API gets increased by one every time a breaking change or new feature is introduced.  
To find out the Protocol API version for a given release, consult its source code/documentation.

We have no regular release schedule. Releases can be released at any time.

# Releasing for Developers

1. Make sure that the version field in `common/include/NintendontVersion.h` have been bumped
2. Make sure that if the remote protocol API changed, that the API version also got bumped.
3. Create and push a tag onto the `master` branch. The tag is `v[RELEASE_NUMBER]-multiworld`. E.g. `v10-multiworld`.
4. Wait for CI to create the release. Then edit the release description and fill it in with GitHub's auto-generated release notes.
