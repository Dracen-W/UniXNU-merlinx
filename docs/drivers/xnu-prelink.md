# XnuPrelink

Start with the [adaptation procedure](/porting/platform) for board inputs, source changes, build integration and verification order.

Kernel prelink emitter and ABI tooling.

[Source repository](https://github.com/UniXNU/XnuPrelink).

XnuPrelink binds the built XNU kernel to selected original kexts and writes the prelink plan. The OS profile supplies the kernelcache pin, roots, slide, and ABI settings.

## Integration

The catalog records implementation scope, hardware and OS requirements, board dependencies, revision, build stage, outputs, and runtime identifiers. The selected profile orders build commands by dependency.

```sh
./unixnu build prelink -D ../UniXNU-sheng
```

[Component catalog](/reference/components)
