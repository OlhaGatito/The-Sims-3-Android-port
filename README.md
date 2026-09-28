# The Sims 3 — Android Port

Porting research and Linux ARM loader for the Android version of **The Sims 3**.

## Public repository scope

This repository contains only technical artifacts developed for the port:

- source code;
- loader;
- build files;
- launcher scripts;
- compiled port/loader binaries;
- permitted technical `.unpacked` artifacts used by the loader;
- documentation and configuration.

### Proprietary game data is not included

The repository must never contain:

- APK files;
- OBB files;
- extracted APK/OBB game data or assets;
- original proprietary libraries/assets;
- saves, dumps or equivalent proprietary content.

Game data required for testing is supplied separately by the user and remains outside the repository.

## Current project

The current loader targets the Marmalade/S3E executable found in the Android release.

Architecture and runtime details are documented as the implementation advances.

## License

See [LICENSE](LICENSE).
