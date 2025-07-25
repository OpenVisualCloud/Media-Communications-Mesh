# Running tests with FFmpeg handlers

> **Note:** This document is a stub. It is planned to be extended later on.

This document describes how to prepare FFmpeg classes in order to execute a test using FFmpeg handlers.

## Concept

### FFmpegExecutor Structure

| Component | Type | Description |
|-----------|------|-------------|
| `ff` | `FFmpeg` | Main FFmpeg object containing input/output configuration |
| `host` | `str` | Host configuration |

### FFmpeg Object Structure

| Parameter | Type | Description |
|-----------|------|-------------|
| `prefix_variables` | `dict()` | Dictionary with key=value pairs added at command beginning |
| `ffmpeg_path` | `str` | Path to FFmpeg executable |
| `ffmpeg_input` | `FFmpegIO()` | Input configuration object |
| `ffmpeg_output` | `FFmpegIO()` | Output configuration object |
| `yes_overwrite` | `bool` | Whether to overwrite existing files |

Object of an `FFmpeg` class is directly responsible of building the commands, used by `FFmpegExecutor` and should be treated as an encapsulating element consisting of mainly two components - `ffmpeg_input` and `ffmpeg_output` - both of `FFmpegIO` class or dependent (child class).

`prefix_variables` is a dictionary with keys and values that are added at the beginning of the command with `key=value` (with a space at the end). `ffmpeg_path` is used to determine a path to a specific FFmpeg executable (by default: `ffmpeg` - which means the one available in `$PATH`).

> Notes:
>
> 1. It is not possible to execute more than a single input and output per instance at the moment. This should be achievable using filters, but they are not implemented yet.
> 2. The child classes to FFmpegIO can be seen on the [FFmpegIO class and subclasses](README.md#ffmpegio-class-and-subclasses) graph.
