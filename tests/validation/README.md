# Media Communications Mesh API validation

This part of the repository is to be used solely for holding validation applications for Media Communications Mesh API and other files related to validating mentioned API.

## Compilation process

> **Note:** At the moment, it is not automated to expedite the development process.

In order to compile the apps, first compile the whole Media Communications Mesh using `build.sh` script from the root of the repo, then use following commands:

`cc -o <output_name> <input_file.c> -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`

Examples:
- `cc -o sender_app video_sender_app.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`
- `cc -o recver_app video_recver_app.c -lbsd $(pwd)/../../_build/lib/libmcm_dp.so`
