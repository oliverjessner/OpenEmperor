# Reverse-engineering notes

This directory records independently observed facts, their evidence and provenance, and explicit unknowns. It is separate from implementation code. Do not add original assets, proprietary source, or decompiled code here.

The user supplies legally obtained game files, initially from the GOG distribution; this repository does not decode the installer. See [file formats](file-formats.md), the [research log](research-log.md), the [read-only map profile](map-format.md), and the [targeted graphics-ID study](graphics-id.md). Document observed bytes and unresolved semantics without embedding proprietary files or treating reference implementations as copyable source.

[Map first-draw pass 1](map-first-draw.md) records the post-load saved-ID clear, bounded water/elevation selector trace, signed height operand, landscape provenance and remaining visual acceptance limits.
