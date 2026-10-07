# Isolated Tolk Runtime Source

This directory contains the source used to build the Tolk runtime bundled with
`foo_output_device_switcher`.

The source is based on the Tolk fork used by this project and is distributed
under the LGPLv3 terms in `LICENSE.txt`. The NVDA client license is included in
`LICENSE-NVDA.txt`.

Upstream project:

```text
https://github.com/boz700908/tolk
```

The component-specific build changes the screen-reader driver file names to
the `ods-`/`odssa`/`odsdol` names used by this component. The Tolk loader also
resolves every driver relative to the loaded Tolk module with an absolute
path. This keeps the runtime independent from other foobar2000 components
that use Tolk.
